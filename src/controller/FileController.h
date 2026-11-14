#pragma once

#include <drogon/HttpController.h>

#include <functional>
#include <string>

/**
 * @brief 教案文件 HTTP 接口层
 *
 * 只负责：解析请求 -> 调用 Service -> 统一封装响应；
 * 业务规则、状态机、数据库操作均在 Service / DAO / Provider 层。
 */
class FileController : public drogon::HttpController<FileController>
{
  public:
    METHOD_LIST_BEGIN
    // 上传（multipart/form-data，字段名 file）
    ADD_METHOD_TO(FileController::upload, "/api/files/upload", drogon::Post);
    // 文件列表（分页 + 过滤）
    ADD_METHOD_TO(FileController::list, "/api/files", drogon::Get);
    // 文件详情
    ADD_METHOD_TO(FileController::detail, "/api/files/{1}", drogon::Get);
    // 查询文件状态
    ADD_METHOD_TO(FileController::status, "/api/files/{1}/status", drogon::Get);
    // 触发 AI 解析
    ADD_METHOD_TO(FileController::parse, "/api/files/{1}/parse", drogon::Post);
    // 查询解析结果
    ADD_METHOD_TO(FileController::parseResult,
                  "/api/files/{1}/parse-result",
                  drogon::Get);
    METHOD_LIST_END

    void upload(const drogon::HttpRequestPtr &request,
                std::function<void(const drogon::HttpResponsePtr &)> &&callback);

    void list(const drogon::HttpRequestPtr &request,
              std::function<void(const drogon::HttpResponsePtr &)> &&callback);

    void detail(const drogon::HttpRequestPtr &request,
                std::function<void(const drogon::HttpResponsePtr &)> &&callback,
                std::string fileId);

    void status(const drogon::HttpRequestPtr &request,
                std::function<void(const drogon::HttpResponsePtr &)> &&callback,
                std::string fileId);

    void parse(const drogon::HttpRequestPtr &request,
               std::function<void(const drogon::HttpResponsePtr &)> &&callback,
               std::string fileId);

    void parseResult(
        const drogon::HttpRequestPtr &request,
        std::function<void(const drogon::HttpResponsePtr &)> &&callback,
        std::string fileId);

  private:
    /// 从查询串读取参数（不存在时返回空字符串）
    static std::string queryParam(const drogon::HttpRequestPtr &request,
                                  const std::string &key);
};
