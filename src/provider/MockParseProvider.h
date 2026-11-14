#pragma once

#include <string>

#include "config/AppConfig.h"
#include "provider/ParseProvider.h"

/**
 * @brief Mock 解析实现（默认启用）
 *
 * 特点：
 *   - 通过定时器异步模拟真实模型耗时（可配置区间）；
 *   - 按 success_rate 随机成功/失败，文件名命中 fail_keyword 时强制失败，便于测试；
 *   - 解析结果由“文件名 + 文件大小 + 文件内容片段”计算出的哈希派生，
 *     同一文件结果稳定、不同文件结果不同，不是写死的固定文本。
 */
class MockParseProvider : public ParseProvider
{
  public:
    explicit MockParseProvider(MockConfig config = MockConfig());

    std::string modelType() const override
    {
        return "mock";
    }

    std::string modelName() const override
    {
        return "mock-v1";
    }

    void parse(const ParseInput &input, ParseCallback callback) override;

  private:
    /// 基于哈希派生结构化内容
    ParseOutput buildOutput(const ParseInput &input, uint64_t seed) const;

    MockConfig config_;
};
