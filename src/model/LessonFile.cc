#include "model/LessonFile.h"

Json::Value LessonFile::toJson() const
{
    Json::Value json;
    json["fileId"] = fileId;
    json["originalName"] = originalName;
    json["fileType"] = fileType;
    json["fileSize"] = static_cast<Json::Int64>(fileSize);
    json["uploader"] = uploader;
    json["status"] = fileStatusToString(status);
    json["statusText"] = fileStatusText(status);
    json["createdAt"] = createdAt;
    json["updatedAt"] = updatedAt;
    return json;
}
