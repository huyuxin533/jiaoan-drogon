#pragma once

#include <drogon/orm/DbClient.h>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "model/LessonFile.h"

/**
 * @brief 文件列表查询条件
 */
struct FileQuery
{
    int page{1};
    int pageSize{10};
    std::string status;    ///< 状态过滤，空表示全部
    std::string uploader;  ///< 上传人过滤，空表示全部
    std::string keyword;   ///< 原始文件名模糊搜索，空表示全部
};

/**
 * @brief lesson_file 数据访问对象
 *
 * 全部使用 Drogon ORM 异步接口，回调在数据库线程池执行；
 * 错误通过 error 参数回传，由 Service 统一转换为业务错误。
 */
class LessonFileDao
{
  public:
    using InsertCallback =
        std::function<void(std::optional<int64_t> id, std::string error)>;
    using FindCallback =
        std::function<void(std::optional<LessonFile>, std::string error)>;
    using ListCallback = std::function<void(std::vector<LessonFile> files,
                                            int64_t total,
                                            std::string error)>;
    using UpdateCallback =
        std::function<void(int64_t affectedRows, std::string error)>;

    void insert(const LessonFile &file, InsertCallback callback);

    void findByFileId(const std::string &fileId, FindCallback callback);

    void list(const FileQuery &query, ListCallback callback);

    /// 无条件更新状态（用于解析完成后回写）
    void updateStatus(const std::string &fileId,
                      FileStatus status,
                      UpdateCallback callback);

    /**
     * @brief 原子地把 PENDING/FAILED 抢占为 PARSING
     *
     * 依赖单条 UPDATE 的原子性，避免并发触发重复解析。
     * affectedRows == 0 说明状态已被其它请求改变。
     */
    void claimForParsing(const std::string &fileId, UpdateCallback callback);

  private:
    static drogon::orm::DbClientPtr db();
};
