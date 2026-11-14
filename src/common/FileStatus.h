#pragma once

#include <string>

/// 教案文件状态（与数据库 status 字段一一对应）
enum class FileStatus
{
    Pending,  ///< 待解析
    Parsing,  ///< 解析中
    Success,  ///< 解析成功
    Failed,   ///< 解析失败
};

/// 单次解析结果状态（lesson_parse_result.parse_status）
enum class ParseStatus
{
    Success,
    Failed,
};

/// 数据库/接口使用的状态字符串：PENDING / PARSING / SUCCESS / FAILED
const char *fileStatusToString(FileStatus status);

/// 中文展示文案：待解析 / 解析中 / 解析成功 / 解析失败
const char *fileStatusText(FileStatus status);

/**
 * @brief 状态字符串转枚举
 * @param text 状态字符串（大小写不敏感）
 * @param ok   可选输出参数，转换是否成功
 */
FileStatus fileStatusFromString(const std::string &text, bool *ok = nullptr);

const char *parseStatusToString(ParseStatus status);

ParseStatus parseStatusFromString(const std::string &text, bool *ok = nullptr);
