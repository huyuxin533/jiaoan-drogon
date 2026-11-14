#include "service/FileService.h"

#include <drogon/drogon.h>
#include <drogon/utils/Utilities.h>

#include <filesystem>
#include <fstream>

#include "common/BizException.h"
#include "common/StringUtil.h"
#include "config/AppConfig.h"

namespace
{
constexpr int kMaxPageSize = 100;
constexpr int kMaxKeywordLength = 100;

std::string sanitizeFileName(const std::string &rawName)
{
    std::string name = util::baseName(rawName);
    std::string cleaned;
    cleaned.reserve(name.size());
    for (unsigned char ch : name)
    {
        if (ch < 0x20 || ch == 0x7F)
        {
            continue;
        }
        cleaned.push_back(static_cast<char>(ch));
    }
    return util::trim(cleaned);
}

int parsePositiveInt(const std::string &text,
                     const std::string &field,
                     int fallback,
                     int maxValue)
{
    if (util::trim(text).empty())
    {
        return fallback;
    }
    int value = 0;
    if (!util::tryParseInt(text, value) || value < 1 || value > maxValue)
    {
        throw BizException(ApiCode::InvalidParam,
                           field + " 必须为 1~" + std::to_string(maxValue) +
                               " 之间的整数");
    }
    return value;
}
}  // namespace

FileService &FileService::instance()
{
    static FileService service;
    return service;
}

void FileService::upload(const UploadRequest &request,
                         std::function<void(ServiceResult<LessonFile>)> callback)
{
    const auto &storage = AppConfig::instance().storage();

    // ---------------- 参数校验 ----------------
    const std::string originalName = sanitizeFileName(request.originalName);
    if (originalName.empty())
    {
        throw BizException(ApiCode::InvalidParam, "文件名不能为空");
    }
    if (originalName.size() > 255)
    {
        throw BizException(ApiCode::InvalidParam, "文件名过长，最多 255 字节");
    }

    const std::string extension = util::fileExtension(originalName);
    if (extension.empty() ||
        !AppConfig::instance().isAllowedExtension(extension))
    {
        throw BizException(ApiCode::UnsupportedFileType,
                           "仅支持 PDF、DOC、DOCX 格式，当前文件扩展名为 " +
                               (extension.empty() ? std::string("<无>")
                                                  : extension));
    }

    if (request.data == nullptr || request.length == 0)
    {
        throw BizException(ApiCode::InvalidParam, "文件内容为空");
    }
    if (static_cast<int64_t>(request.length) > storage.maxFileSize)
    {
        throw BizException(
            ApiCode::FileTooLarge,
            "文件大小超过限制（最大 " +
                std::to_string(storage.maxFileSize / 1024 / 1024) + "MB）");
    }

    const std::string uploader = util::trim(request.uploader);
    if (!util::isValidUploader(uploader))
    {
        throw BizException(ApiCode::InvalidParam,
                           "uploader 不合法：1~64 个可见字符，且不能包含 / \\ 等特殊字符");
    }

    // ---------------- 本地落盘 ----------------
    const std::string fileId = drogon::utils::getUuid();
    const std::string relativePath = saveToLocal(request, fileId, extension);

    LessonFile file = buildEntity(request, relativePath);
    file.fileId = fileId;
    file.originalName = originalName;
    file.fileType = util::toUpper(extension);
    file.fileSize = static_cast<int64_t>(request.length);
    file.uploader = uploader;
    file.status = FileStatus::Pending;

    const std::string absolutePath =
        (std::filesystem::path(AppConfig::instance().absoluteStorageRoot()) /
         relativePath)
            .string();

    // ---------------- 数据库写入 ----------------
    fileDao_.insert(
        file,
        [this, callback, file, absolutePath](std::optional<int64_t> id,
                                             std::string error) mutable {
            if (!error.empty())
            {
                LOG_ERROR << "写入 lesson_file 失败: " << error;
                // 数据库失败时回滚已落盘文件，避免产生孤儿文件
                std::error_code ec;
                std::filesystem::remove(absolutePath, ec);
                callback(ServiceResult<LessonFile>::failure(
                    ApiCode::InternalError, "文件信息保存失败，请稍后重试"));
                return;
            }
            file.id = id.value_or(0);

            // 回查一次，拿到数据库生成的创建/更新时间
            fileDao_.findByFileId(
                file.fileId,
                [callback, file](std::optional<LessonFile> saved,
                                 std::string findError) mutable {
                    if (findError.empty() && saved.has_value())
                    {
                        callback(
                            ServiceResult<LessonFile>::success(std::move(*saved)));
                        return;
                    }
                    callback(ServiceResult<LessonFile>::success(std::move(file)));
                });
        });
}

std::string FileService::saveToLocal(const UploadRequest &request,
                                     const std::string &fileId,
                                     const std::string &extension) const
{
    const std::filesystem::path relativePath =
        std::filesystem::path(util::currentDatePath()) /
        (fileId + "." + extension);
    const std::filesystem::path absolutePath =
        std::filesystem::path(AppConfig::instance().absoluteStorageRoot()) /
        relativePath;

    std::error_code ec;
    std::filesystem::create_directories(absolutePath.parent_path(), ec);
    if (ec)
    {
        LOG_ERROR << "创建上传目录失败: " << absolutePath.parent_path()
                  << ", error=" << ec.message();
        throw BizException(ApiCode::StorageError, "创建存储目录失败");
    }

    std::ofstream stream(absolutePath,
                         std::ios::binary | std::ios::out | std::ios::trunc);
    if (!stream.is_open())
    {
        LOG_ERROR << "打开本地文件失败: " << absolutePath;
        throw BizException(ApiCode::StorageError, "本地文件写入失败");
    }
    stream.write(request.data, static_cast<std::streamsize>(request.length));
    stream.flush();
    if (!stream.good())
    {
        stream.close();
        std::filesystem::remove(absolutePath, ec);
        LOG_ERROR << "写入本地文件失败: " << absolutePath;
        throw BizException(ApiCode::StorageError, "本地文件写入失败");
    }

    LOG_INFO << "文件已保存: fileId=" << fileId
             << ", path=" << absolutePath.string()
             << ", size=" << request.length;
    return relativePath.generic_string();
}

LessonFile FileService::buildEntity(const UploadRequest &request,
                                    const std::string &relativePath) const
{
    LessonFile file;
    file.originalName = util::baseName(request.originalName);
    file.fileSize = static_cast<int64_t>(request.length);
    file.storagePath = relativePath;
    file.uploader = util::trim(request.uploader);
    file.status = FileStatus::Pending;
    return file;
}

void FileService::list(
    const ListQueryParams &params,
    std::function<void(ServiceResult<PageResult<LessonFile>>)> callback)
{
    FileQuery query;
    query.page = parsePositiveInt(params.page, "page", 1, 1000000);
    query.pageSize = parsePositiveInt(params.pageSize, "pageSize", 10,
                                      kMaxPageSize);

    if (!util::trim(params.status).empty())
    {
        bool ok = false;
        const FileStatus status = fileStatusFromString(params.status, &ok);
        if (!ok)
        {
            throw BizException(
                ApiCode::InvalidParam,
                "status 取值非法，仅支持 PENDING / PARSING / SUCCESS / FAILED");
        }
        query.status = fileStatusToString(status);
    }

    query.uploader = util::trim(params.uploader);
    if (query.uploader.size() > 64)
    {
        throw BizException(ApiCode::InvalidParam, "uploader 最多 64 字节");
    }

    query.keyword = util::trim(params.keyword);
    if (query.keyword.size() > kMaxKeywordLength)
    {
        throw BizException(ApiCode::InvalidParam,
                           "keyword 最多 " + std::to_string(kMaxKeywordLength) +
                               " 字节");
    }

    const int page = query.page;
    const int pageSize = query.pageSize;

    fileDao_.list(
        query,
        [callback, page, pageSize](std::vector<LessonFile> files,
                                   int64_t total,
                                   std::string error) {
            if (!error.empty())
            {
                LOG_ERROR << "查询文件列表失败: " << error;
                callback(ServiceResult<PageResult<LessonFile>>::failure(
                    ApiCode::InternalError, "查询文件列表失败"));
                return;
            }
            PageResult<LessonFile> pageResult;
            pageResult.items = std::move(files);
            pageResult.total = total;
            pageResult.page = page;
            pageResult.pageSize = pageSize;
            callback(
                ServiceResult<PageResult<LessonFile>>::success(std::move(pageResult)));
        });
}

void FileService::detail(const std::string &fileId,
                         std::function<void(ServiceResult<LessonFile>)> callback)
{
    if (!util::isValidId(fileId))
    {
        throw BizException(ApiCode::InvalidParam, "fileId 格式非法");
    }

    fileDao_.findByFileId(
        fileId,
        [callback](std::optional<LessonFile> file, std::string error) {
            if (!error.empty())
            {
                LOG_ERROR << "查询文件详情失败: " << error;
                callback(ServiceResult<LessonFile>::failure(
                    ApiCode::InternalError, "查询文件详情失败"));
                return;
            }
            if (!file.has_value())
            {
                callback(ServiceResult<LessonFile>::failure(
                    ApiCode::NotFound, "文件不存在"));
                return;
            }
            callback(ServiceResult<LessonFile>::success(std::move(*file)));
        });
}
