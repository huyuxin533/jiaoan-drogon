#include "dao/LessonParseResultDao.h"

#include <drogon/drogon.h>
#include <drogon/orm/Exception.h>

#include "common/FileStatus.h"
#include "dao/RowMapper.h"

namespace
{
const char *kSelectColumns =
    "id, file_id, lesson_title, grade_subject, teaching_objectives, keywords, "
    "summary, model_type, model_name, cost_ms, parse_status, fail_reason, "
    "created_at, updated_at";
}  // namespace

drogon::orm::DbClientPtr LessonParseResultDao::db()
{
    return drogon::app().getDbClient();
}

void LessonParseResultDao::insert(const LessonParseResult &result,
                                  InsertCallback callback)
{
    auto client = db();
    if (!client)
    {
        callback("数据库客户端未初始化");
        return;
    }

    client->execSqlAsync(
        "INSERT INTO lesson_parse_result "
        "(file_id, lesson_title, grade_subject, teaching_objectives, keywords, "
        " summary, model_type, model_name, cost_ms, parse_status, fail_reason) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
        [callback](const drogon::orm::Result &) { callback(""); },
        [callback](const drogon::orm::DrogonDbException &error) {
            callback(error.base().what());
        },
        result.fileId,
        result.lessonTitle,
        result.gradeSubject,
        result.teachingObjectives,
        result.keywords,
        result.summary,
        result.modelType,
        result.modelName,
        result.costMs,
        std::string(parseStatusToString(result.parseStatus)),
        result.failReason);
}

void LessonParseResultDao::findLatest(const std::string &fileId,
                                      FindCallback callback)
{
    auto client = db();
    if (!client)
    {
        callback(std::nullopt, "数据库客户端未初始化");
        return;
    }

    const std::string sql = std::string("SELECT ") + kSelectColumns +
                            " FROM lesson_parse_result WHERE file_id = ? "
                            "ORDER BY id DESC LIMIT 1";

    client->execSqlAsync(
        sql,
        [callback](const drogon::orm::Result &result) {
            if (result.size() == 0)
            {
                callback(std::nullopt, "");
                return;
            }
            callback(mapper::lessonParseResult(result[0]), "");
        },
        [callback](const drogon::orm::DrogonDbException &error) {
            callback(std::nullopt, error.base().what());
        },
        fileId);
}
