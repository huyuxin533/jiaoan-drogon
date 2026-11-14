#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "common/ApiCode.h"

/// 无返回值场景使用的占位类型
struct Empty
{
};

/**
 * @brief Service 层统一返回值
 *
 * 异步流程（数据库回调、解析回调）无法抛出异常，因此统一用该结构传递结果；
 * 同步参数校验仍可直接抛 BizException，由 Controller 层统一捕获。
 */
template <typename T>
class ServiceResult
{
  public:
    ServiceResult() = default;

    static ServiceResult success(T value)
    {
        ServiceResult result;
        result.ok_ = true;
        result.value_ = std::move(value);
        return result;
    }

    static ServiceResult failure(ApiCode code, std::string message)
    {
        ServiceResult result;
        result.ok_ = false;
        result.code_ = code;
        result.message_ = std::move(message);
        return result;
    }

    bool isOk() const
    {
        return ok_;
    }

    ApiCode code() const
    {
        return ok_ ? ApiCode::Ok : code_;
    }

    const std::string &message() const
    {
        return message_;
    }

    const T &value() const
    {
        return value_;
    }

    T &value()
    {
        return value_;
    }

  private:
    bool ok_{false};
    T value_{};
    ApiCode code_{ApiCode::InternalError};
    std::string message_;
};

/// 分页结果
template <typename T>
struct PageResult
{
    std::vector<T> items;
    int64_t total{0};
    int page{1};
    int pageSize{10};

    int totalPages() const
    {
        if (pageSize <= 0)
        {
            return 0;
        }
        return static_cast<int>((total + pageSize - 1) / pageSize);
    }
};
