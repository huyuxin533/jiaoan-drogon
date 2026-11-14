#pragma once

#include <string>

#include "provider/ParseTypes.h"

/**
 * @brief AI 解析能力抽象基类
 *
 * 责任：把“一份教案文件”解析为结构化字段，不感知数据库与 HTTP。
 * 约定：
 *   - parse() 为异步接口，必须且只能回调一次；
 *   - 具体实现自行处理超时/异常，并把失败原因写入 ParseOutput.failReason。
 */
class ParseProvider
{
  public:
    virtual ~ParseProvider() = default;

    /// 模型标识：mock / real（写入 lesson_parse_result.model_type）
    virtual std::string modelType() const = 0;

    /// 模型名称：如 mock-v1、qwen-plus
    virtual std::string modelName() const = 0;

    virtual void parse(const ParseInput &input, ParseCallback callback) = 0;
};
