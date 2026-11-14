#pragma once

#include <memory>

#include "config/AppConfig.h"
#include "provider/ParseProvider.h"

/**
 * @brief 解析器工厂
 *
 * 根据配置返回 Mock 或 Real 实现，Service 层只依赖 ParseProvider 抽象。
 */
class ParseProviderFactory
{
  public:
    static std::shared_ptr<ParseProvider> create(const ParseConfig &config);
};
