#include <drogon/drogon.h>

#include <filesystem>
#include <iostream>
#include <string>

#include "common/ApiCode.h"
#include "common/ApiResponse.h"
#include "config/AppConfig.h"

namespace
{
void printUsage(const char *program)
{
    std::cout << "用法: " << program << " [-c|--config <config.json>]\n"
              << "示例: " << program << " -c ./config.json\n";
}
}  // namespace

int main(int argc, char *argv[])
{
    std::string configPath = "config.json";
    for (int i = 1; i < argc; ++i)
    {
        const std::string argument = argv[i];
        if ((argument == "-c" || argument == "--config") && i + 1 < argc)
        {
            configPath = argv[++i];
        }
        else if (argument == "-h" || argument == "--help")
        {
            printUsage(argv[0]);
            return 0;
        }
    }

    std::error_code ec;
    if (!std::filesystem::exists(configPath, ec))
    {
        std::cerr << "配置文件不存在: " << configPath << std::endl;
        return 1;
    }

    // 1. 加载 Drogon 配置（监听端口、数据库连接等）
    drogon::app().loadConfigFile(configPath);

    // 2. 加载自定义业务配置
    AppConfig::instance().load(drogon::app().getCustomConfig());
    const auto &storage = AppConfig::instance().storage();

    // 3. 请求体大小限制
    //    - setClientMaxBodySize：允许上传 50MB 文件（含 multipart 开销 64MB）
    //    - setClientMaxMemoryBodySize：保证 multipart 请求体整体驻留内存，
    //      便于 MultiPartParser 同步解析（生产环境可改为流式落盘）
    drogon::app().setClientMaxBodySize(
        static_cast<size_t>(storage.maxBodySize));
    drogon::app().setClientMaxMemoryBodySize(
        static_cast<size_t>(storage.maxBodySize));

    // 4. 确保本地存储目录存在
    const std::string uploadRoot = AppConfig::instance().absoluteStorageRoot();
    std::filesystem::create_directories(uploadRoot, ec);
    if (ec)
    {
        std::cerr << "创建文件存储目录失败: " << uploadRoot
                  << ", error=" << ec.message() << std::endl;
        return 1;
    }
    LOG_INFO << "本地文件存储目录: " << uploadRoot;

    // 5. 全局异常兜底：未捕获异常统一返回 5000
    drogon::app().setExceptionHandler(
        [](const std::exception &exception,
           const drogon::HttpRequestPtr &request,
           std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
            LOG_ERROR << "未捕获异常: " << exception.what()
                      << ", path=" << (request ? request->path() : "-");
            callback(api::fail(ApiCode::InternalError,
                              "服务器内部错误",
                              drogon::k500InternalServerError));
        });

    // 6. 统一 404 返回体（使用默认处理器，保证 timestamp 为本次请求时间）
    drogon::app().setDefaultHandler(
        [](const drogon::HttpRequestPtr &request,
           std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
            LOG_WARN << "接口不存在: " << (request ? request->path() : "-");
            callback(api::fail(ApiCode::NotFound,
                               "接口不存在",
                               drogon::k404NotFound));
        });

    LOG_INFO << "AI 教案解析服务启动，监听 0.0.0.0:8080";
    drogon::app().run();
    return 0;
}
