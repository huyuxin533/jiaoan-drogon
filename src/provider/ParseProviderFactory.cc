#include "provider/ParseProviderFactory.h"

#include <drogon/drogon.h>

#include "provider/MockParseProvider.h"
#include "provider/RealParseProvider.h"

std::shared_ptr<ParseProvider> ParseProviderFactory::create(
    const ParseConfig &config)
{
    if (config.provider == "real")
    {
        LOG_INFO << "使用真实大模型解析器: model=" << config.real.model;
        return std::make_shared<RealParseProvider>(config.real);
    }

    LOG_INFO << "使用 Mock 解析器: success_rate=" << config.mock.successRate
             << ", delay=[" << config.mock.minDelayMs << ", "
             << config.mock.maxDelayMs << "]ms";
    return std::make_shared<MockParseProvider>(config.mock);
}
