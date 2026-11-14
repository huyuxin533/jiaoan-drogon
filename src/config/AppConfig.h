#pragma once

#include <json/json.h>

#include <cstdint>
#include <string>
#include <vector>

/// 本地文件存储配置
struct StorageConfig
{
    std::string rootDir{"./uploads"};             ///< 文件根目录
    int64_t maxFileSize{50LL * 1024 * 1024};      ///< 单文件大小上限：50MB
    int64_t maxBodySize{64LL * 1024 * 1024};      ///< HTTP 请求体上限：64MB
    std::vector<std::string> allowedExtensions{"pdf", "doc", "docx"};
};

/// Mock 解析器配置
struct MockConfig
{
    double successRate{0.8};       ///< 随机成功率
    int minDelayMs{300};           ///< 模拟最短耗时
    int maxDelayMs{1500};          ///< 模拟最长耗时
    std::string failKeyword{"fail"};  ///< 文件名命中该关键字时强制失败（便于测试）
};

/// 真实大模型配置（预留）
struct RealConfig
{
    std::string baseUrl;  ///< 例如 https://api.openai.com（不含路径）
    std::string apiKey;
    std::string model{"qwen-plus"};
    std::string chatPath{"/v1/chat/completions"};  ///< OpenAI 兼容的补全路径
    int maxExcerptBytes{4096};                     ///< 送给模型的正文片段上限
};

struct ParseConfig
{
    std::string provider{"mock"};  ///< mock / real
    MockConfig mock;
    RealConfig real;
};

/**
 * @brief 应用级配置（来自 config.json 的 custom_config 节点）
 *
 * 单例，main() 中加载一次；读取失败时回落到内置默认值并打印告警。
 */
class AppConfig
{
  public:
    static AppConfig &instance();

    void load(const Json::Value &customConfig);

    const StorageConfig &storage() const
    {
        return storage_;
    }

    const ParseConfig &parse() const
    {
        return parse_;
    }

    /// 判断扩展名是否在白名单内（大小写不敏感）
    bool isAllowedExtension(const std::string &extension) const;

    /// 文件存储根目录的绝对路径
    std::string absoluteStorageRoot() const;

  private:
    AppConfig() = default;

    StorageConfig storage_;
    ParseConfig parse_;
};
