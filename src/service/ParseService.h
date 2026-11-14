#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "common/ServiceResult.h"
#include "dao/LessonFileDao.h"
#include "dao/LessonParseResultDao.h"
#include "model/LessonFile.h"
#include "model/LessonParseResult.h"
#include "provider/ParseProvider.h"

/// 触发解析后的即时返回信息
struct ParseTriggerInfo
{
    std::string fileId;
    FileStatus status{FileStatus::Parsing};
    std::string provider;   ///< mock / real
    std::string modelName;  ///< mock-v1 / qwen-plus
};

/// 解析结果查询信息（覆盖待解析/解析中/成功/失败四种情况）
struct ParseResultInfo
{
    std::string fileId;
    std::string originalName;
    FileStatus status{FileStatus::Pending};
    bool finished{false};
    bool success{false};
    std::string message;
    std::optional<LessonParseResult> result;
};

/**
 * @brief 解析业务服务
 *
 * 负责状态机：
 *   PENDING/FAILED --claim--> PARSING --provider--> SUCCESS / FAILED
 * 并负责把 Provider 输出写入 lesson_parse_result。
 */
class ParseService
{
  public:
    static ParseService &instance();

    void trigger(const std::string &fileId,
                 std::function<void(ServiceResult<ParseTriggerInfo>)> callback);

    void getResult(const std::string &fileId,
                   std::function<void(ServiceResult<ParseResultInfo>)> callback);

  private:
    ParseService();

    /// 异步执行解析，并在完成后回写结果与文件状态
    void startParsing(const LessonFile &file);

    LessonFileDao fileDao_;
    LessonParseResultDao resultDao_;
    std::shared_ptr<ParseProvider> provider_;
};
