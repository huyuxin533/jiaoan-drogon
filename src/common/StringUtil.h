#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace util
{
/// 去除首尾空白字符
std::string trim(const std::string &text);

std::string toLower(std::string text);

std::string toUpper(std::string text);

bool iequals(const std::string &lhs, const std::string &rhs);

/// 大小写不敏感的子串判断（UTF-8 中文按字节比较，可正常工作）
bool containsIgnoreCase(const std::string &text, const std::string &needle);

/// 去掉目录部分，仅保留文件名（防止路径穿越）
std::string baseName(const std::string &path);

/// 扩展名（不含点，统一小写），如 "pdf"
std::string fileExtension(const std::string &fileName);

/// 去掉扩展名的文件名
std::string fileStem(const std::string &fileName);

/// 把字符串中的连续空白/下划线/短横线规范化为单个空格
std::string normalizeName(const std::string &name);

/// 当前日期目录，如 "2026/11/13"
std::string currentDatePath();

/// 当前毫秒时间戳
int64_t nowMs();

/// 严格的整数解析（允许首尾空白），失败返回 false
bool tryParseInt(const std::string &text, int &value);

/// 校验业务 ID（UUID/字符串 ID）：非空、<=64、仅允许字母数字和短横线
bool isValidId(const std::string &id);

/// 校验上传人：1~64 字节可见字符，禁止路径分隔符等特殊字符
bool isValidUploader(const std::string &uploader);

/// 转义 LIKE 通配符（配合 ESCAPE '\\' 使用）
std::string escapeLike(const std::string &text);

/// 按分隔符拆分（默认英文逗号），自动去空白并丢弃空项
std::vector<std::string> split(const std::string &text, char delimiter = ',');

/// 用分隔符拼接
std::string join(const std::vector<std::string> &items, const std::string &delimiter);
}  // namespace util
