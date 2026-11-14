#include "config/AppConfig.h"

#include <drogon/drogon.h>

#include <algorithm>
#include <filesystem>

#include "common/StringUtil.h"

namespace
{
std::string readString(const Json::Value &node,
                       const char *key,
                       const std::string &fallback)
{
    if (!node.isObject() || !node.isMember(key) || !node[key].isString())
    {
        return fallback;
    }
    return node[key].asString();
}

int64_t readInt64(const Json::Value &node, const char *key, int64_t fallback)
{
    if (!node.isObject() || !node.isMember(key))
    {
        return fallback;
    }
    const Json::Value &value = node[key];
    if (value.isInt64() || value.isInt() || value.isUInt())
    {
        return value.asInt64();
    }
    if (value.isDouble())
    {
        return static_cast<int64_t>(value.asDouble());
    }
    return fallback;
}

double readDouble(const Json::Value &node, const char *key, double fallback)
{
    if (!node.isObject() || !node.isMember(key))
    {
        return fallback;
    }
    const Json::Value &value = node[key];
    if (value.isDouble() || value.isInt() || value.isInt64())
    {
        return value.asDouble();
    }
    return fallback;
}
}  // namespace

AppConfig &AppConfig::instance()
{
    static AppConfig config;
    return config;
}

void AppConfig::load(const Json::Value &customConfig)
{
    if (!customConfig.isObject())
    {
        LOG_WARN << "custom_config 不存在，使用内置默认配置";
        return;
    }

    // ---------------- storage ----------------
    const Json::Value &storage = customConfig["storage"];
    storage_.rootDir = readString(storage, "root_dir", storage_.rootDir);
    storage_.maxFileSize =
        readInt64(storage, "max_file_size", storage_.maxFileSize);
    storage_.maxBodySize =
        readInt64(storage, "max_body_size", storage_.maxBodySize);

    if (storage.isObject() && storage["allowed_extensions"].isArray() &&
        !storage["allowed_extensions"].empty())
    {
        std::vector<std::string> extensions;
        for (const auto &item : storage["allowed_extensions"])
        {
            if (item.isString())
            {
                const std::string ext = util::toLower(util::trim(item.asString()));
                if (!ext.empty())
                {
                    extensions.push_back(ext);
                }
            }
        }
        if (!extensions.empty())
        {
            storage_.allowedExtensions = std::move(extensions);
        }
    }

    if (storage_.maxFileSize <= 0)
    {
        storage_.maxFileSize = 50LL * 1024 * 1024;
    }
    // 请求体上限必须大于单文件上限，预留 multipart 边界开销
    if (storage_.maxBodySize <= storage_.maxFileSize)
    {
        storage_.maxBodySize = storage_.maxFileSize + 8LL * 1024 * 1024;
    }

    // ---------------- parse ----------------
    const Json::Value &parse = customConfig["parse"];
    parse_.provider =
        util::toLower(util::trim(readString(parse, "provider", parse_.provider)));
    if (parse_.provider != "mock" && parse_.provider != "real")
    {
        LOG_WARN << "未知的 parse.provider=" << parse_.provider
                 << "，回落到 mock";
        parse_.provider = "mock";
    }

    const Json::Value &mock = parse["mock"];
    parse_.mock.successRate = readDouble(mock, "success_rate", 0.8);
    if (parse_.mock.successRate < 0.0)
    {
        parse_.mock.successRate = 0.0;
    }
    if (parse_.mock.successRate > 1.0)
    {
        parse_.mock.successRate = 1.0;
    }
    parse_.mock.minDelayMs =
        static_cast<int>(readInt64(mock, "min_delay_ms", 300));
    parse_.mock.maxDelayMs =
        static_cast<int>(readInt64(mock, "max_delay_ms", 1500));
    if (parse_.mock.minDelayMs < 0)
    {
        parse_.mock.minDelayMs = 0;
    }
    if (parse_.mock.maxDelayMs < parse_.mock.minDelayMs)
    {
        parse_.mock.maxDelayMs = parse_.mock.minDelayMs;
    }
    parse_.mock.failKeyword =
        readString(mock, "fail_keyword", parse_.mock.failKeyword);

    const Json::Value &real = parse["real"];
    parse_.real.baseUrl = util::trim(readString(real, "base_url", ""));
    parse_.real.apiKey = util::trim(readString(real, "api_key", ""));
    parse_.real.model = readString(real, "model", parse_.real.model);
    parse_.real.chatPath =
        readString(real, "chat_path", parse_.real.chatPath);
    parse_.real.maxExcerptBytes = static_cast<int>(
        readInt64(real, "max_excerpt_bytes", parse_.real.maxExcerptBytes));
    if (parse_.real.maxExcerptBytes <= 0)
    {
        parse_.real.maxExcerptBytes = 4096;
    }
    // 兼容 base_url 末尾带 / 的写法
    while (!parse_.real.baseUrl.empty() &&
           parse_.real.baseUrl.back() == '/')
    {
        parse_.real.baseUrl.pop_back();
    }
    if (!parse_.real.chatPath.empty() && parse_.real.chatPath.front() != '/')
    {
        parse_.real.chatPath.insert(parse_.real.chatPath.begin(), '/');
    }

    LOG_INFO << "配置加载完成: storage.root_dir=" << absoluteStorageRoot()
             << ", max_file_size=" << storage_.maxFileSize
             << ", max_body_size=" << storage_.maxBodySize
             << ", parse.provider=" << parse_.provider
             << ", mock.success_rate=" << parse_.mock.successRate;
}

bool AppConfig::isAllowedExtension(const std::string &extension) const
{
    const std::string lower = util::toLower(util::trim(extension));
    return std::find(storage_.allowedExtensions.begin(),
                     storage_.allowedExtensions.end(),
                     lower) != storage_.allowedExtensions.end();
}

std::string AppConfig::absoluteStorageRoot() const
{
    std::error_code ec;
    const auto absolute = std::filesystem::absolute(storage_.rootDir, ec);
    if (ec)
    {
        return storage_.rootDir;
    }
    return absolute.lexically_normal().string();
}
