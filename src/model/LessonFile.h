#pragma once

#include <json/json.h>

#include <cstdint>
#include <string>

#include "common/FileStatus.h"

/**
 * @brief 教案文件实体，对应 lesson_file 表
 */
struct LessonFile
{
    int64_t id{0};                ///< 自增主键（内部使用）
    std::string fileId;           ///< 对外文件ID（UUID）
    std::string originalName;     ///< 原始文件名
    std::string fileType;         ///< PDF / DOC / DOCX
    int64_t fileSize{0};          ///< 字节数
    std::string storagePath;      ///< 相对存储路径，如 2026/11/13/xxx.pdf
    std::string uploader;         ///< 上传人
    FileStatus status{FileStatus::Pending};
    std::string createdAt;
    std::string updatedAt;

    /// 对外 JSON（不暴露服务器本地存储路径）
    Json::Value toJson() const;
};
