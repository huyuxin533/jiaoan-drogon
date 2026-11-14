#include "common/StringUtil.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <sstream>

namespace util
{
namespace
{
bool isSpaceChar(unsigned char ch)
{
    return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\f' ||
           ch == '\v';
}
}  // namespace

std::string trim(const std::string &text)
{
    size_t begin = 0;
    size_t end = text.size();
    while (begin < end && isSpaceChar(static_cast<unsigned char>(text[begin])))
    {
        ++begin;
    }
    while (end > begin && isSpaceChar(static_cast<unsigned char>(text[end - 1])))
    {
        --end;
    }
    return text.substr(begin, end - begin);
}

std::string toLower(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return text;
}

std::string toUpper(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });
    return text;
}

bool iequals(const std::string &lhs, const std::string &rhs)
{
    if (lhs.size() != rhs.size())
    {
        return false;
    }
    for (size_t i = 0; i < lhs.size(); ++i)
    {
        if (std::tolower(static_cast<unsigned char>(lhs[i])) !=
            std::tolower(static_cast<unsigned char>(rhs[i])))
        {
            return false;
        }
    }
    return true;
}

bool containsIgnoreCase(const std::string &text, const std::string &needle)
{
    if (needle.empty())
    {
        return true;
    }
    if (text.size() < needle.size())
    {
        return false;
    }
    const std::string lowerText = toLower(text);
    const std::string lowerNeedle = toLower(needle);
    return lowerText.find(lowerNeedle) != std::string::npos;
}

std::string baseName(const std::string &path)
{
    std::string normalized = path;
    // 同时兼容 Windows 风格的反斜杠
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    const auto pos = normalized.find_last_of('/');
    if (pos == std::string::npos)
    {
        return trim(normalized);
    }
    return trim(normalized.substr(pos + 1));
}

std::string fileExtension(const std::string &fileName)
{
    const std::string name = baseName(fileName);
    const auto pos = name.find_last_of('.');
    if (pos == std::string::npos || pos + 1 >= name.size())
    {
        return "";
    }
    return toLower(name.substr(pos + 1));
}

std::string fileStem(const std::string &fileName)
{
    const std::string name = baseName(fileName);
    const auto pos = name.find_last_of('.');
    if (pos == std::string::npos)
    {
        return name;
    }
    return name.substr(0, pos);
}

std::string normalizeName(const std::string &name)
{
    std::string result;
    result.reserve(name.size());
    bool lastWasSpace = false;
    for (unsigned char ch : name)
    {
        if (ch == '_' || ch == '-' || isSpaceChar(ch))
        {
            if (!result.empty() && !lastWasSpace)
            {
                result.push_back(' ');
                lastWasSpace = true;
            }
            continue;
        }
        result.push_back(static_cast<char>(ch));
        lastWasSpace = false;
    }
    return trim(result);
}

std::string currentDatePath()
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t seconds = std::chrono::system_clock::to_time_t(now);
    std::tm local{};
#if defined(_WIN32)
    localtime_s(&local, &seconds);
#else
    localtime_r(&seconds, &local);
#endif
    char buffer[32] = {0};
    std::snprintf(buffer,
                  sizeof(buffer),
                  "%04d/%02d/%02d",
                  local.tm_year + 1900,
                  local.tm_mon + 1,
                  local.tm_mday);
    return std::string(buffer);
}

int64_t nowMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

bool tryParseInt(const std::string &text, int &value)
{
    const std::string trimmed = trim(text);
    if (trimmed.empty())
    {
        return false;
    }
    try
    {
        size_t consumed = 0;
        const long long parsed = std::stoll(trimmed, &consumed);
        if (consumed != trimmed.size() || parsed < INT32_MIN ||
            parsed > INT32_MAX)
        {
            return false;
        }
        value = static_cast<int>(parsed);
        return true;
    }
    catch (const std::exception &)
    {
        return false;
    }
}

bool isValidId(const std::string &id)
{
    if (id.empty() || id.size() > 64)
    {
        return false;
    }
    for (unsigned char ch : id)
    {
        const bool allowed = (ch >= '0' && ch <= '9') ||
                             (ch >= 'a' && ch <= 'z') ||
                             (ch >= 'A' && ch <= 'Z') || ch == '-' || ch == '_';
        if (!allowed)
        {
            return false;
        }
    }
    return true;
}

bool isValidUploader(const std::string &uploader)
{
    const std::string trimmed = trim(uploader);
    if (trimmed.empty() || trimmed.size() > 64)
    {
        return false;
    }
    for (unsigned char ch : trimmed)
    {
        if (ch < 0x20 || ch == 0x7F)
        {
            return false;
        }
        switch (ch)
        {
            case '/':
            case '\\':
            case '\'':
            case '"':
            case ';':
            case '<':
            case '>':
            case '|':
                return false;
            default:
                break;
        }
    }
    return true;
}

std::string escapeLike(const std::string &text)
{
    std::string escaped;
    escaped.reserve(text.size() + 8);
    for (char ch : text)
    {
        if (ch == '\\' || ch == '%' || ch == '_')
        {
            escaped.push_back('\\');
        }
        escaped.push_back(ch);
    }
    return escaped;
}

std::vector<std::string> split(const std::string &text, char delimiter)
{
    std::vector<std::string> parts;
    std::string current;
    std::istringstream stream(text);
    while (std::getline(stream, current, delimiter))
    {
        const std::string item = trim(current);
        if (!item.empty())
        {
            parts.push_back(item);
        }
    }
    return parts;
}

std::string join(const std::vector<std::string> &items,
                 const std::string &delimiter)
{
    std::string result;
    for (size_t i = 0; i < items.size(); ++i)
    {
        if (i > 0)
        {
            result += delimiter;
        }
        result += items[i];
    }
    return result;
}
}  // namespace util
