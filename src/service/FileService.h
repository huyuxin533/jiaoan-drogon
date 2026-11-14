#pragma once

#include <functional>
#include <string>

#include "common/ServiceResult.h"
#include "dao/LessonFileDao.h"
#include "model/LessonFile.h"

/**
 * @brief 上传请求（由 Controller 从 multipart 中提取）
 *
 * @note data 指向框架请求体内存，仅在 upload() 同步执行期间有效；
 *       Service 会在返回前完成落盘。
 */
struct UploadRequest
{
    std::string originalName;
    std::string uploader;
    const char *data{nullptr};
    size_t length{0};
};

/**
 * @brief 列表查询原始参数（字符串，由 Service 统一校验）
 */
struct ListQueryParams
{
    std::string page;
    std::string pageSize;
    std::string status;
    std::string uploader;
    std::string keyword;
};

/**
 * @brief 文件业务服务
 *
 * 负责：参数校验、本地文件落盘、数据库读写编排、错误转换。
 * 不处理 HTTP 细节（请求解析、响应封装由 Controller 完成）。
 */
class FileService
{
  public:
    static FileService &instance();

    void upload(const UploadRequest &request,
                std::function<void(ServiceResult<LessonFile>)> callback);

    void list(const ListQueryParams &params,
              std::function<void(ServiceResult<PageResult<LessonFile>>)> callback);

    void detail(const std::string &fileId,
                std::function<void(ServiceResult<LessonFile>)> callback);

  private:
    FileService() = default;

    /// 校验参数（校验失败抛 BizException）
    LessonFile buildEntity(const UploadRequest &request,
                           const std::string &relativePath) const;

    /// 同步写入本地磁盘，返回相对路径；失败抛 BizException
    std::string saveToLocal(const UploadRequest &request,
                            const std::string &fileId,
                            const std::string &extension) const;

    LessonFileDao fileDao_;
};
