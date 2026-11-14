#pragma once

#include <drogon/HttpTypes.h>

#include <exception>
#include <string>
#include <utility>

#include "common/ApiCode.h"

/**
 * @brief 业务异常
 *
 * 用于在同步流程中快速中断并向上传递“可预期”的业务错误，
 * 由 Controller 层统一捕获后转换为标准返回体。
 */
class BizException : public std::exception
{
  public:
    BizException(ApiCode code,
                 std::string message,
                 drogon::HttpStatusCode httpStatus = drogon::k200OK)
        : code_(code), message_(std::move(message)), httpStatus_(httpStatus)
    {
    }

    ApiCode code() const noexcept
    {
        return code_;
    }

    drogon::HttpStatusCode httpStatus() const noexcept
    {
        return httpStatus_;
    }

    const char *what() const noexcept override
    {
        return message_.c_str();
    }

  private:
    ApiCode code_;
    std::string message_;
    drogon::HttpStatusCode httpStatus_;
};
