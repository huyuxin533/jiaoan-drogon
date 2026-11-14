#pragma once

#include <cstdint>
#include <functional>
#include <string>

/**
 * @brief 解析输入：由 Service 从数据库实体转换而来
 */
struct ParseInput
{
    std::string fileId;
    std::string originalName;
    std::string fileType;
    std::string storagePath;  ///< 本地绝对路径
    int64_t fileSize{0};
};

/**
 * @brief 解析输出：Provider 的统一产出
 *
 * 成功时填充结构化字段；失败时只保证 failReason / modelType / modelName 有值。
 */
struct ParseOutput
{
    bool success{false};
    std::string lessonTitle;
    std::string gradeSubject;
    std::string teachingObjectives;
    std::string keywords;  ///< 英文逗号分隔
    std::string summary;
    std::string modelType{"mock"};  ///< mock / real
    std::string modelName;
    int64_t costMs{0};
    std::string failReason;

    static ParseOutput failure(std::string reason,
                               std::string modelType,
                               std::string modelName)
    {
        ParseOutput output;
        output.success = false;
        output.failReason = std::move(reason);
        output.modelType = std::move(modelType);
        output.modelName = std::move(modelName);
        return output;
    }
};

/// Provider 必须且只能回调一次
using ParseCallback = std::function<void(ParseOutput)>;
