#include "dao/LessonFileDao.h"

#include <drogon/drogon.h>
#include <drogon/orm/Exception.h>

#include "common/StringUtil.h"
#include "dao/RowMapper.h"

namespace
{
const char *kSelectColumns =
    "id, file_id, original_name, file_type, file_size, storage_path, "
    "uploader, status, created_at, updated_at";

/// 列表/计数共用的过滤条件：空字符串参数表示不过滤
const char *kFilterClause =
    " WHERE (? = '' OR status = ?)"
    "   AND (? = '' OR uploader = ?)"
    "   AND (? = '' OR original_name LIKE ?)";
}  // namespace

drogon::orm::DbClientPtr LessonFileDao::db()
{
    return drogon::app().getDbClient();
}

void LessonFileDao::insert(const LessonFile &file, InsertCallback callback)
{
    auto client = db();
    if (!client)
    {
        callback(std::nullopt, "数据库客户端未初始化");
        return;
    }

    client->execSqlAsync(
        "INSERT INTO lesson_file "
        "(file_id, original_name, file_type, file_size, storage_path, uploader, status) "
        "VALUES (?, ?, ?, ?, ?, ?, ?)",
        [callback](const drogon::orm::Result &result) {
            callback(static_cast<int64_t>(result.insertId()), "");
        },
        [callback](const drogon::orm::DrogonDbException &error) {
            callback(std::nullopt, error.base().what());
        },
        file.fileId,
        file.originalName,
        file.fileType,
        static_cast<long long>(file.fileSize),
        file.storagePath,
        file.uploader,
        std::string(fileStatusToString(file.status)));
}

void LessonFileDao::findByFileId(const std::string &fileId,
                                 FindCallback callback)
{
    auto client = db();
    if (!client)
    {
        callback(std::nullopt, "数据库客户端未初始化");
        return;
    }

    const std::string sql = std::string("SELECT ") + kSelectColumns +
                            " FROM lesson_file WHERE file_id = ? LIMIT 1";

    client->execSqlAsync(
        sql,
        [callback](const drogon::orm::Result &result) {
            if (result.size() == 0)
            {
                callback(std::nullopt, "");
                return;
            }
            callback(mapper::lessonFile(result[0]), "");
        },
        [callback](const drogon::orm::DrogonDbException &error) {
            callback(std::nullopt, error.base().what());
        },
        fileId);
}

void LessonFileDao::list(const FileQuery &query, ListCallback callback)
{
    auto client = db();
    if (!client)
    {
        callback({}, 0, "数据库客户端未初始化");
        return;
    }

    const std::string status = util::toUpper(util::trim(query.status));
    const std::string uploader = util::trim(query.uploader);
    const std::string likePattern =
        "%" + util::escapeLike(util::trim(query.keyword)) + "%";

    const std::string countSql =
        std::string("SELECT COUNT(*) AS total FROM lesson_file") + kFilterClause;
    const std::string listSql = std::string("SELECT ") + kSelectColumns +
                                " FROM lesson_file" + kFilterClause +
                                " ORDER BY id DESC LIMIT ? OFFSET ?";

    const int page = query.page;
    const int pageSize = query.pageSize;
    const int64_t offset =
        static_cast<int64_t>(page - 1) * static_cast<int64_t>(pageSize);

    auto errorHandler = [callback](const drogon::orm::DrogonDbException &error) {
        callback({}, 0, error.base().what());
    };

    client->execSqlAsync(
        countSql,
        [client,
         callback,
         listSql,
         status,
         uploader,
         likePattern,
         pageSize,
         offset,
         errorHandler](const drogon::orm::Result &countResult) {
            int64_t total = 0;
            if (countResult.size() > 0)
            {
                total = mapper::number(countResult[0], "total");
            }
            client->execSqlAsync(
                listSql,
                [callback, total](const drogon::orm::Result &rows) {
                    std::vector<LessonFile> files;
                    files.reserve(rows.size());
                    for (const auto &row : rows)
                    {
                        files.push_back(mapper::lessonFile(row));
                    }
                    callback(std::move(files), total, "");
                },
                errorHandler,
                status,
                status,
                uploader,
                uploader,
                likePattern,
                likePattern,
                pageSize,
                static_cast<long long>(offset));
        },
        errorHandler,
        status,
        status,
        uploader,
        uploader,
        likePattern,
        likePattern);
}

void LessonFileDao::updateStatus(const std::string &fileId,
                                 FileStatus status,
                                 UpdateCallback callback)
{
    auto client = db();
    if (!client)
    {
        callback(0, "数据库客户端未初始化");
        return;
    }

    client->execSqlAsync(
        "UPDATE lesson_file SET status = ?, updated_at = CURRENT_TIMESTAMP(3) "
        "WHERE file_id = ?",
        [callback](const drogon::orm::Result &result) {
            callback(static_cast<int64_t>(result.affectedRows()), "");
        },
        [callback](const drogon::orm::DrogonDbException &error) {
            callback(0, error.base().what());
        },
        std::string(fileStatusToString(status)),
        fileId);
}

void LessonFileDao::claimForParsing(const std::string &fileId,
                                    UpdateCallback callback)
{
    auto client = db();
    if (!client)
    {
        callback(0, "数据库客户端未初始化");
        return;
    }

    client->execSqlAsync(
        "UPDATE lesson_file SET status = 'PARSING', "
        "updated_at = CURRENT_TIMESTAMP(3) "
        "WHERE file_id = ? AND status IN ('PENDING', 'FAILED')",
        [callback](const drogon::orm::Result &result) {
            callback(static_cast<int64_t>(result.affectedRows()), "");
        },
        [callback](const drogon::orm::DrogonDbException &error) {
            callback(0, error.base().what());
        },
        fileId);
}
