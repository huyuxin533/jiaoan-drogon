#pragma once

#include <drogon/HttpResponse.h>
#include <json/json.h>

#include <string>

#include "common/ApiCode.h"
#include "common/BizException.h"
#include "common/StringUtil.h"

/**
 * @brief 统一返回封装
 *
 * 返回体结构：
 * {
 *   "code": 0,
 *   "message": "ok",
 *   "data": { ... },
 *   "timestamp": 1762992000000
 * }
 */
namespace api
{
inline Json::Value makeBody(int code,
                            const std::string &message,
                            const Json::Value &data)
{
    Json::Value body;
    body["code"] = code;
    body["message"] = message;
    body["data"] = data.isNull() ? Json::Value(Json::objectValue) : data;
    body["timestamp"] = static_cast<Json::Int64>(util::nowMs());
    return body;
}

inline drogon::HttpResponsePtr ok(const Json::Value &data = Json::Value(),
                                  const std::string &message = "ok")
{
    auto response = drogon::HttpResponse::newHttpJsonResponse(
        makeBody(apiCodeValue(ApiCode::Ok), message, data));
    response->setStatusCode(drogon::k200OK);
    return response;
}

inline drogon::HttpResponsePtr fail(
    ApiCode code,
    const std::string &message,
    drogon::HttpStatusCode httpStatus = drogon::k200OK)
{
    auto response = drogon::HttpResponse::newHttpJsonResponse(
        makeBody(apiCodeValue(code), message, Json::Value(Json::objectValue)));
    response->setStatusCode(httpStatus);
    return response;
}

inline drogon::HttpResponsePtr fail(const BizException &exception)
{
    return fail(exception.code(), exception.what(), exception.httpStatus());
}
}  // namespace api
