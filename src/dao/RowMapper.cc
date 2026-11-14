#include "dao/RowMapper.h"

#include <drogon/orm/Field.h>

namespace mapper
{
std::string text(const drogon::orm::Row &row, const char *column)
{
    const auto &field = row[column];
    if (field.isNull())
    {
        return "";
    }
    return field.as<std::string>();
}

int64_t number(const drogon::orm::Row &row, const char *column)
{
    const auto &field = row[column];
    if (field.isNull())
    {
        return 0;
    }
    return static_cast<int64_t>(field.as<long long>());
}

int integer(const drogon::orm::Row &row, const char *column)
{
    return static_cast<int>(number(row, column));
}

LessonFile lessonFile(const drogon::orm::Row &row)
{
    LessonFile file;
    file.id = number(row, "id");
    file.fileId = text(row, "file_id");
    file.originalName = text(row, "original_name");
    file.fileType = text(row, "file_type");
    file.fileSize = number(row, "file_size");
    file.storagePath = text(row, "storage_path");
    file.uploader = text(row, "uploader");
    file.status = fileStatusFromString(text(row, "status"));
    file.createdAt = text(row, "created_at");
    file.updatedAt = text(row, "updated_at");
    return file;
}

LessonParseResult lessonParseResult(const drogon::orm::Row &row)
{
    LessonParseResult result;
    result.id = number(row, "id");
    result.fileId = text(row, "file_id");
    result.lessonTitle = text(row, "lesson_title");
    result.gradeSubject = text(row, "grade_subject");
    result.teachingObjectives = text(row, "teaching_objectives");
    result.keywords = text(row, "keywords");
    result.summary = text(row, "summary");
    result.modelType = text(row, "model_type");
    result.modelName = text(row, "model_name");
    result.costMs = integer(row, "cost_ms");
    result.parseStatus = parseStatusFromString(text(row, "parse_status"));
    result.failReason = text(row, "fail_reason");
    result.createdAt = text(row, "created_at");
    result.updatedAt = text(row, "updated_at");
    return result;
}
}  // namespace mapper
