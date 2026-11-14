#include "model/LessonParseResult.h"

#include "common/StringUtil.h"

Json::Value LessonParseResult::toJson() const
{
    Json::Value json;
    json["fileId"] = fileId;
    json["lessonTitle"] = lessonTitle;
    json["gradeSubject"] = gradeSubject;
    json["teachingObjectives"] = teachingObjectives;

    // 关键词在库中以英文逗号分隔，对外输出为数组
    Json::Value keywordArray(Json::arrayValue);
    for (const auto &item : util::split(keywords))
    {
        keywordArray.append(item);
    }
    json["keywords"] = keywordArray;

    json["summary"] = summary;
    json["modelType"] = modelType;
    json["modelName"] = modelName;
    json["costMs"] = costMs;
    json["parseStatus"] = parseStatusToString(parseStatus);
    json["createdAt"] = createdAt;
    json["updatedAt"] = updatedAt;
    return json;
}
