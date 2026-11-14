#pragma once

#include <drogon/drogon.h>

#include <exception>
#include <functional>
#include <utility>

#include "common/ApiCode.h"
#include "common/ApiResponse.h"
#include "common/BizException.h"

namespace api
{
/**
 * @brief Controller 层统一异常兜底
 *
 * 业务异常 -> 对应业务码；其它异常 -> 5000，并打印错误日志。
 */
template <typename Handler>
void guarded(const std::function<void(const drogon::HttpResponsePtr &)> &callback,
             Handler &&handler)
{
    try
    {
        std::forward<Handler>(handler)();
    }
    catch (const BizException &exception)
    {
        LOG_WARN << "business error: code=" << apiCodeValue(exception.code())
                 << ", message=" << exception.what();
        callback(fail(exception));
    }
    catch (const std::exception &exception)
    {
        LOG_ERROR << "unhandled exception: " << exception.what();
        callback(fail(ApiCode::InternalError,
                      "服务器内部错误",
                      drogon::k500InternalServerError));
    }
    catch (...)
    {
        LOG_ERROR << "unhandled unknown exception";
        callback(fail(ApiCode::InternalError,
                      "服务器内部错误",
                      drogon::k500InternalServerError));
    }
}
}  // namespace api
