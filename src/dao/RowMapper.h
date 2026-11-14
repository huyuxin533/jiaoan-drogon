#pragma once

#include <drogon/orm/Row.h>

#include <cstdint>
#include <string>

#include "model/LessonFile.h"
#include "model/LessonParseResult.h"

/**
 * @brief 数据库行 -> 实体映射
 *
 * 集中处理 NULL 字段与类型转换，避免 SQL 细节散落到 Service 层。
 */
namespace mapper
{
std::string text(const drogon::orm::Row &row, const char *column);

int64_t number(const drogon::orm::Row &row, const char *column);

int integer(const drogon::orm::Row &row, const char *column);

LessonFile lessonFile(const drogon::orm::Row &row);

LessonParseResult lessonParseResult(const drogon::orm::Row &row);
}  // namespace mapper
