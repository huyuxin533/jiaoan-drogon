#pragma once

#include <drogon/orm/DbClient.h>

#include <functional>
#include <optional>
#include <string>

#include "model/LessonParseResult.h"

/**
 * @brief lesson_parse_result 数据访问对象
 */
class LessonParseResultDao
{
  public:
    using InsertCallback = std::function<void(std::string error)>;
    using FindCallback =
        std::function<void(std::optional<LessonParseResult>, std::string error)>;

    /// 每次解析写入一条新记录，保留历史
    void insert(const LessonParseResult &result, InsertCallback callback);

    /// 查询某文件最近一次解析结果
    void findLatest(const std::string &fileId, FindCallback callback);

  private:
    static drogon::orm::DbClientPtr db();
};
