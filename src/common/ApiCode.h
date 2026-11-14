#pragma once

#include <string>

/**
 * @brief 统一业务错误码
 *
 * 约定：
 *   - 0 表示成功；
 *   - 1xxx 为客户端/业务错误（HTTP 状态码仍返回 200，由 code 区分）；
 *   - 5xxx 为服务端错误。
 */
enum class ApiCode : int
{
    Ok = 0,                    ///< 成功
    InvalidParam = 1001,       ///< 参数校验失败
    UnsupportedFileType = 1002,///< 文件格式不支持
    FileTooLarge = 1003,       ///< 文件超过大小限制
    NotFound = 1004,           ///< 资源不存在
    StatusConflict = 1005,     ///< 状态冲突（重复解析等）
    StorageError = 1006,       ///< 本地文件存储失败
    ParseFailed = 1007,        ///< 解析失败
    InternalError = 5000,      ///< 服务内部错误
};

inline int apiCodeValue(ApiCode code)
{
    return static_cast<int>(code);
}

inline std::string defaultMessage(ApiCode code)
{
    switch (code)
    {
        case ApiCode::Ok:
            return "ok";
        case ApiCode::InvalidParam:
            return "参数校验失败";
        case ApiCode::UnsupportedFileType:
            return "文件格式不支持";
        case ApiCode::FileTooLarge:
            return "文件超过大小限制";
        case ApiCode::NotFound:
            return "资源不存在";
        case ApiCode::StatusConflict:
            return "当前状态不允许该操作";
        case ApiCode::StorageError:
            return "文件存储失败";
        case ApiCode::ParseFailed:
            return "解析失败";
        case ApiCode::InternalError:
            return "服务器内部错误";
    }
    return "未知错误";
}
