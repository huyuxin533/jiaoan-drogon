#include "service/ParseService.h"

#include <drogon/drogon.h>

#include <filesystem>
#include <utility>

#include "common/BizException.h"
#include "common/StringUtil.h"
#include "config/AppConfig.h"
#include "provider/ParseProviderFactory.h"

ParseService &ParseService::instance()
{
    static ParseService service;
    return service;
}

ParseService::ParseService()
    : provider_(ParseProviderFactory::create(AppConfig::instance().parse()))
{
}

void ParseService::trigger(
    const std::string &fileId,
    std::function<void(ServiceResult<ParseTriggerInfo>)> callback)
{
    if (!util::isValidId(fileId))
    {
        throw BizException(ApiCode::InvalidParam, "fileId 格式非法");
    }

    fileDao_.findByFileId(
        fileId,
        [this, callback, fileId](std::optional<LessonFile> file,
                                 std::string error) {
            if (!error.empty())
            {
                LOG_ERROR << "触发解析前查询文件失败: " << error;
                callback(ServiceResult<ParseTriggerInfo>::failure(
                    ApiCode::InternalError, "查询文件失败"));
                return;
            }
            if (!file.has_value())
            {
                callback(ServiceResult<ParseTriggerInfo>::failure(
                    ApiCode::NotFound, "文件不存在"));
                return;
            }
            if (file->status == FileStatus::Parsing)
            {
                callback(ServiceResult<ParseTriggerInfo>::failure(
                    ApiCode::StatusConflict, "文件正在解析中，请勿重复触发"));
                return;
            }
            if (file->status == FileStatus::Success)
            {
                callback(ServiceResult<ParseTriggerInfo>::failure(
                    ApiCode::StatusConflict, "文件已解析成功，无需重复解析"));
                return;
            }

            // 原子抢占：只有 PENDING/FAILED 才能进入 PARSING
            fileDao_.claimForParsing(
                fileId,
                [this, callback, file = *file](int64_t affectedRows,
                                               std::string claimError) mutable {
                    if (!claimError.empty())
                    {
                        LOG_ERROR << "更新解析状态失败: " << claimError;
                        callback(ServiceResult<ParseTriggerInfo>::failure(
                            ApiCode::InternalError, "更新解析状态失败"));
                        return;
                    }
                    if (affectedRows == 0)
                    {
                        callback(ServiceResult<ParseTriggerInfo>::failure(
                            ApiCode::StatusConflict,
                            "文件状态已变更，请刷新后重试"));
                        return;
                    }

                    ParseTriggerInfo info;
                    info.fileId = file.fileId;
                    info.status = FileStatus::Parsing;
                    info.provider = provider_->modelType();
                    info.modelName = provider_->modelName();
                    callback(ServiceResult<ParseTriggerInfo>::success(info));

                    // 立即返回“解析中”，实际解析在后台异步完成
                    startParsing(file);
                });
        });
}

void ParseService::startParsing(const LessonFile &file)
{
    ParseInput input;
    input.fileId = file.fileId;
    input.originalName = file.originalName;
    input.fileType = file.fileType;
    input.fileSize = file.fileSize;
    input.storagePath =
        (std::filesystem::path(AppConfig::instance().absoluteStorageRoot()) /
         file.storagePath)
            .string();

    LOG_INFO << "开始解析: fileId=" << file.fileId
             << ", provider=" << provider_->modelType()
             << ", model=" << provider_->modelName();

    auto provider = provider_;
    provider->parse(
        input,
        [this, file, provider](ParseOutput output) {
            LessonParseResult result;
            result.fileId = file.fileId;
            result.modelType =
                output.modelType.empty() ? provider->modelType() : output.modelType;
            result.modelName =
                output.modelName.empty() ? provider->modelName() : output.modelName;
            result.costMs = static_cast<int>(output.costMs);
            result.parseStatus =
                output.success ? ParseStatus::Success : ParseStatus::Failed;

            if (output.success)
            {
                result.lessonTitle = output.lessonTitle;
                result.gradeSubject = output.gradeSubject;
                result.teachingObjectives = output.teachingObjectives;
                result.keywords = output.keywords;
                result.summary = output.summary;
            }
            else
            {
                result.failReason = output.failReason.empty()
                                        ? "解析失败（未返回具体原因）"
                                        : output.failReason;
            }

            LOG_INFO << "解析完成: fileId=" << file.fileId
                     << ", success=" << output.success
                     << ", costMs=" << result.costMs
                     << ", reason=" << result.failReason;

            resultDao_.insert(
                result,
                [this, file, output](std::string insertError) {
                    bool success = output.success;
                    if (!insertError.empty())
                    {
                        LOG_ERROR << "写入 lesson_parse_result 失败: "
                                  << insertError;
                        // 结果落库失败时按失败处理，避免出现“成功但无结果”
                        success = false;
                    }

                    const FileStatus finalStatus =
                        success ? FileStatus::Success : FileStatus::Failed;
                    fileDao_.updateStatus(
                        file.fileId,
                        finalStatus,
                        [file, finalStatus](int64_t affectedRows,
                                            std::string updateError) {
                            if (!updateError.empty())
                            {
                                LOG_ERROR << "回写文件状态失败: fileId="
                                          << file.fileId
                                          << ", error=" << updateError;
                                return;
                            }
                            LOG_INFO << "文件状态已更新: fileId=" << file.fileId
                                     << ", status="
                                     << fileStatusToString(finalStatus)
                                     << ", affected=" << affectedRows;
                        });
                });
        });
}

void ParseService::getResult(
    const std::string &fileId,
    std::function<void(ServiceResult<ParseResultInfo>)> callback)
{
    if (!util::isValidId(fileId))
    {
        throw BizException(ApiCode::InvalidParam, "fileId 格式非法");
    }

    fileDao_.findByFileId(
        fileId,
        [this, callback](std::optional<LessonFile> file, std::string error) {
            if (!error.empty())
            {
                LOG_ERROR << "查询解析结果前读取文件失败: " << error;
                callback(ServiceResult<ParseResultInfo>::failure(
                    ApiCode::InternalError, "查询文件失败"));
                return;
            }
            if (!file.has_value())
            {
                callback(ServiceResult<ParseResultInfo>::failure(
                    ApiCode::NotFound, "文件不存在"));
                return;
            }

            ParseResultInfo info;
            info.fileId = file->fileId;
            info.originalName = file->originalName;
            info.status = file->status;

            if (file->status == FileStatus::Pending)
            {
                info.finished = false;
                info.message = "文件尚未触发解析";
                callback(ServiceResult<ParseResultInfo>::success(info));
                return;
            }
            if (file->status == FileStatus::Parsing)
            {
                info.finished = false;
                info.message = "解析中，请稍后重试";
                callback(ServiceResult<ParseResultInfo>::success(info));
                return;
            }

            resultDao_.findLatest(
                file->fileId,
                [callback, info](std::optional<LessonParseResult> result,
                                 std::string findError) mutable {
                    if (!findError.empty())
                    {
                        LOG_ERROR << "查询解析结果失败: " << findError;
                        callback(ServiceResult<ParseResultInfo>::failure(
                            ApiCode::InternalError, "查询解析结果失败"));
                        return;
                    }
                    info.finished = true;
                    if (result.has_value())
                    {
                        info.success =
                            (result->parseStatus == ParseStatus::Success);
                        info.message = info.success
                                           ? "解析成功"
                                           : (result->failReason.empty()
                                                  ? "解析失败（未记录失败原因）"
                                                  : result->failReason);
                        info.result = std::move(result);
                    }
                    else
                    {
                        info.message = "未找到解析结果记录，请重新触发解析";
                    }
                    callback(ServiceResult<ParseResultInfo>::success(info));
                });
        });
}
