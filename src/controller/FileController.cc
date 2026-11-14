#include "controller/FileController.h"

#include <drogon/MultiPart.h>
#include <drogon/drogon.h>

#include <string>
#include <utility>

#include "common/ApiGuard.h"
#include "common/ApiResponse.h"
#include "common/BizException.h"
#include "common/StringUtil.h"
#include "service/FileService.h"
#include "service/ParseService.h"

std::string FileController::queryParam(const drogon::HttpRequestPtr &request,
                                       const std::string &key)
{
    const auto value = request->getOptionalParameter<std::string>(key);
    return value.has_value() ? *value : std::string();
}

void FileController::upload(
    const drogon::HttpRequestPtr &request,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback)
{
    api::guarded(callback, [&]() {
        LOG_INFO << "POST /api/files/upload from " << request->peerAddr().toIp();

        drogon::MultiPartParser parser;
        if (parser.parse(request) != 0)
        {
            throw BizException(
                ApiCode::InvalidParam,
                "multipart/form-data 解析失败，请使用 form-data 并指定文件字段 file");
        }

        const auto &files = parser.getFiles();
        if (files.empty())
        {
            throw BizException(ApiCode::InvalidParam,
                               "缺少文件字段 file，或文件内容为空");
        }
        if (files.size() > 1)
        {
            throw BizException(ApiCode::InvalidParam,
                               "一次只能上传一个文件");
        }

        const auto &file = files.front();

        // uploader 优先从 form 参数读取，兼容 query 参数
        std::string uploader;
        const auto &parameters = parser.getParameters();
        const auto it = parameters.find("uploader");
        if (it != parameters.end())
        {
            uploader = it->second;
        }
        else
        {
            uploader = queryParam(request, "uploader");
        }

        UploadRequest uploadRequest;
        uploadRequest.originalName = file.getFileName();
        uploadRequest.uploader = uploader;
        uploadRequest.data = file.fileData();
        uploadRequest.length = file.fileLength();

        FileService::instance().upload(
            uploadRequest,
            [callback](
                ServiceResult<LessonFile> result) {
                if (!result.isOk())
                {
                    callback(api::fail(result.code(), result.message()));
                    return;
                }
                callback(api::ok(result.value().toJson(), "上传成功"));
            });
    });
}

void FileController::list(
    const drogon::HttpRequestPtr &request,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback)
{
    api::guarded(callback, [&]() {
        LOG_INFO << "GET /api/files";

        ListQueryParams params;
        params.page = queryParam(request, "page");
        params.pageSize = queryParam(request, "pageSize");
        params.status = queryParam(request, "status");
        params.uploader = queryParam(request, "uploader");
        params.keyword = queryParam(request, "keyword");

        FileService::instance().list(
            params,
            [callback](
                ServiceResult<PageResult<LessonFile>> result) {
                if (!result.isOk())
                {
                    callback(api::fail(result.code(), result.message()));
                    return;
                }

                const auto &page = result.value();
                Json::Value data;
                Json::Value items(Json::arrayValue);
                for (const auto &file : page.items)
                {
                    items.append(file.toJson());
                }
                data["list"] = items;
                data["total"] = static_cast<Json::Int64>(page.total);
                data["page"] = page.page;
                data["pageSize"] = page.pageSize;
                data["totalPages"] = page.totalPages();
                callback(api::ok(data));
            });
    });
}

void FileController::detail(
    const drogon::HttpRequestPtr &request,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback,
    std::string fileId)
{
    api::guarded(callback, [&]() {
        LOG_INFO << "GET /api/files/" << fileId;
        FileService::instance().detail(
            fileId,
            [callback](ServiceResult<LessonFile> result) {
                if (!result.isOk())
                {
                    callback(api::fail(result.code(), result.message()));
                    return;
                }
                callback(api::ok(result.value().toJson()));
            });
    });
}

void FileController::status(
    const drogon::HttpRequestPtr &request,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback,
    std::string fileId)
{
    api::guarded(callback, [&]() {
        LOG_INFO << "GET /api/files/" << fileId << "/status";

        FileService::instance().detail(
            fileId,
            [callback](ServiceResult<LessonFile> result) {
                if (!result.isOk())
                {
                    callback(api::fail(result.code(), result.message()));
                    return;
                }
                const auto &file = result.value();
                Json::Value data;
                data["fileId"] = file.fileId;
                data["status"] = fileStatusToString(file.status);
                data["statusText"] = fileStatusText(file.status);
                data["updatedAt"] = file.updatedAt;
                callback(api::ok(data));
            });
    });
}

void FileController::parse(
    const drogon::HttpRequestPtr &request,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback,
    std::string fileId)
{
    api::guarded(callback, [&]() {
        LOG_INFO << "POST /api/files/" << fileId << "/parse";

        ParseService::instance().trigger(
            fileId,
            [callback](
                ServiceResult<ParseTriggerInfo> result) {
                if (!result.isOk())
                {
                    callback(api::fail(result.code(), result.message()));
                    return;
                }
                const auto &info = result.value();
                Json::Value data;
                data["fileId"] = info.fileId;
                data["status"] = fileStatusToString(info.status);
                data["statusText"] = fileStatusText(info.status);
                data["provider"] = info.provider;
                data["modelName"] = info.modelName;
                callback(api::ok(data, "解析任务已提交"));
            });
    });
}

void FileController::parseResult(
    const drogon::HttpRequestPtr &request,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback,
    std::string fileId)
{
    api::guarded(callback, [&]() {
        LOG_INFO << "GET /api/files/" << fileId << "/parse-result";

        ParseService::instance().getResult(
            fileId,
            [callback](
                ServiceResult<ParseResultInfo> result) {
                if (!result.isOk())
                {
                    callback(api::fail(result.code(), result.message()));
                    return;
                }

                const auto &info = result.value();
                Json::Value data;
                data["fileId"] = info.fileId;
                data["originalName"] = info.originalName;
                data["status"] = fileStatusToString(info.status);
                data["statusText"] = fileStatusText(info.status);
                data["finished"] = info.finished;
                data["success"] = info.success;
                data["message"] = info.message;
                data["failReason"] =
                    info.result.has_value() ? info.result->failReason
                                            : std::string();
                if (info.result.has_value())
                {
                    data["result"] = info.result->toJson();
                }
                else
                {
                    data["result"] = Json::Value(Json::nullValue);
                }
                callback(api::ok(data));
            });
    });
}
