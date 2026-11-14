#pragma once

#include <string>

#include "config/AppConfig.h"
#include "provider/ParseProvider.h"

/**
 * @brief 真实大模型解析实现（预留，可切换启用）
 *
 * 当前按 OpenAI 兼容协议（/v1/chat/completions）实现：
 *   - 把文件名、类型、大小与正文片段组装为提示词；
 *   - 要求模型只返回 JSON，再映射为 ParseOutput；
 *   - 未配置 base_url / api_key 时返回明确失败原因，不影响 mock 模式。
 *
 * 后续对接真实模型时只需：
 *   1) 在 config.json 中把 parse.provider 改为 real 并填写 base_url / api_key / model；
 *   2) 若供应商协议不同，替换本类中的请求与响应解析即可，Service 层无需改动。
 */
class RealParseProvider : public ParseProvider
{
  public:
    explicit RealParseProvider(RealConfig config);

    std::string modelType() const override
    {
        return "real";
    }

    std::string modelName() const override
    {
        return config_.model;
    }

    void parse(const ParseInput &input, ParseCallback callback) override;

  private:
    RealConfig config_;
};
