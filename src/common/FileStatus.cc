#include "common/FileStatus.h"

#include "common/StringUtil.h"

const char *fileStatusToString(FileStatus status)
{
    switch (status)
    {
        case FileStatus::Pending:
            return "PENDING";
        case FileStatus::Parsing:
            return "PARSING";
        case FileStatus::Success:
            return "SUCCESS";
        case FileStatus::Failed:
            return "FAILED";
    }
    return "PENDING";
}

const char *fileStatusText(FileStatus status)
{
    switch (status)
    {
        case FileStatus::Pending:
            return "待解析";
        case FileStatus::Parsing:
            return "解析中";
        case FileStatus::Success:
            return "解析成功";
        case FileStatus::Failed:
            return "解析失败";
    }
    return "待解析";
}

FileStatus fileStatusFromString(const std::string &text, bool *ok)
{
    const std::string upper = util::toUpper(util::trim(text));
    if (upper == "PENDING")
    {
        if (ok)
        {
            *ok = true;
        }
        return FileStatus::Pending;
    }
    if (upper == "PARSING")
    {
        if (ok)
        {
            *ok = true;
        }
        return FileStatus::Parsing;
    }
    if (upper == "SUCCESS")
    {
        if (ok)
        {
            *ok = true;
        }
        return FileStatus::Success;
    }
    if (upper == "FAILED")
    {
        if (ok)
        {
            *ok = true;
        }
        return FileStatus::Failed;
    }
    if (ok)
    {
        *ok = false;
    }
    return FileStatus::Pending;
}

const char *parseStatusToString(ParseStatus status)
{
    return status == ParseStatus::Success ? "SUCCESS" : "FAILED";
}

ParseStatus parseStatusFromString(const std::string &text, bool *ok)
{
    const std::string upper = util::toUpper(util::trim(text));
    if (upper == "SUCCESS")
    {
        if (ok)
        {
            *ok = true;
        }
        return ParseStatus::Success;
    }
    if (upper == "FAILED")
    {
        if (ok)
        {
            *ok = true;
        }
        return ParseStatus::Failed;
    }
    if (ok)
    {
        *ok = false;
    }
    return ParseStatus::Failed;
}
