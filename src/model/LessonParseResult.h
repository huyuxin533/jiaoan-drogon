#pragma once

#include <json/json.h>

#include <cstdint>
#include <string>

#include "common/FileStatus.h"

/**
 * @brief 教案结构化解析结果，对应 lesson_parse_result 表
 *
 * 每次解析都会新增一条记录（保留历史），接口返回最近一次结果。
 */
struct LessonParseResult
{
    int64_t id{0};
    std::string fileId;               ///< 关联 lesson_file.file_id
    std::string lessonTitle;          ///< 教案标题
    std::string gradeSubject;         ///< 年级学科
    std::string teachingObjectives;   ///< 教学目标
    std::string keywords;             ///< 关键词，英文逗号分隔
    std::string summary;              ///< 摘要
    std::string modelType{"mock"};    ///< mock / real
    std::string modelName;            ///< mock-v1 / qwen-plus ...
    int costMs{0};                    ///< 解析耗时（毫秒）
    ParseStatus parseStatus{ParseStatus::Success};
    std::string failReason;           ///< 失败原因
    std::string createdAt;
    std::string updatedAt;

    Json::Value toJson() const;
};
