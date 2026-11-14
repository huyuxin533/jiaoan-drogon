#include "provider/RealParseProvider.h"

#include <drogon/HttpClient.h>
#include <drogon/drogon.h>

#include <chrono>
#include <fstream>
#include <initializer_list>
#include <memory>
#include <vector>

#include "common/StringUtil.h"

namespace
{
int64_t elapsedMs(const std::chrono::steady_clock::time_point &start)
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - start)
        .count();
}

/// 读取正文片段，过滤不可打印字符（真实项目可替换为 PDF/DOCX 文本抽取组件）
std::string readExcerpt(const std::string &path, int maxBytes)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
    {
        return "";
    }
    std::string buffer;
    buffer.resize(static_cast<size_t>(maxBytes));
    stream.read(&buffer[0], static_cast<std::streamsize>(buffer.size()));
    buffer.resize(static_cast<size_t>(stream.gcount()));

    std::string cleaned;
    cleaned.reserve(buffer.size());
    for (unsigned char ch : buffer)
    {
        if (ch == '\n' || ch == '\t' || (ch >= 0x20 && ch != 0x7F))
        {
            cleaned.push_back(static_cast<char>(ch));
        }
        else
        {
            cleaned.push_back(' ');
        }
    }
    return util::trim(cleaned);
}

/// 去掉 ```json ... ``` 包裹
std::string stripCodeFence(const std::string &content)
{
    std::string text = util::trim(content);
    if (text.rfind("```", 0) == 0)
    {
        const auto firstLineEnd = text.find('\n');
        if (firstLineEnd != std::string::npos)
        {
            text = text.substr(firstLineEnd + 1);
        }
        const auto fenceEnd = text.rfind("```");
        if (fenceEnd != std::string::npos)
        {
            text = text.substr(0, fenceEnd);
        }
    }
    return util::trim(text);
}

std::string pickField(const Json::Value &root,
                      std::initializer_list<const char *> keys)
{
    for (const char *key : keys)
    {
        const Json::Value &value = root[key];
        if (value.isString())
        {
            return util::trim(value.asString());
        }
    }
    return "";
}

std::string joinKeywords(const Json::Value &value)
{
    if (value.isArray())
    {
        std::vector<std::string> items;
        for (const auto &item : value)
        {
            if (item.isString())
            {
                const std::string keyword = util::trim(item.asString());
                if (!keyword.empty())
                {
                    items.push_back(keyword);
                }
            }
        }
        return util::join(items, ",");
    }
    if (value.isString())
    {
        return util::trim(value.asString());
    }
    return "";
}
}  // namespace

RealParseProvider::RealParseProvider(RealConfig config) : config_(std::move(config))
{
}

void RealParseProvider::parse(const ParseInput &input, ParseCallback callback)
{
    const auto started = std::chrono::steady_clock::now();

    if (config_.baseUrl.empty() || config_.apiKey.empty())
    {
        callback(ParseOutput::failure(
            "真实大模型未配置：请填写 parse.real.base_url 与 parse.real.api_key，"
            "或把 parse.provider 改回 mock",
            "real",
            config_.model));
        return;
    }
    if (config_.baseUrl.rfind("http://", 0) != 0 &&
        config_.baseUrl.rfind("https://", 0) != 0)
    {
        callback(ParseOutput::failure(
            "parse.real.base_url 必须以 http:// 或 https:// 开头", "real",
            config_.model));
        return;
    }

    const std::string excerpt =
        readExcerpt(input.storagePath, config_.maxExcerptBytes);

    std::string prompt = "请把下面的教案文件解析为结构化 JSON，只输出 JSON，不要输出解释。\n";
    prompt += "文件名：" + input.originalName + "\n";
    prompt += "文件类型：" + input.fileType + "\n";
    prompt += "文件大小(字节)：" + std::to_string(input.fileSize) + "\n";
    prompt += "正文片段（可能含噪声）：\n" + excerpt + "\n\n";
    prompt +=
        "输出字段要求：\n"
        "lesson_title: 教案标题\n"
        "grade_subject: 年级与学科，如 七年级 / 数学\n"
        "teaching_objectives: 教学目标，按知识与技能/过程与方法/情感态度分行\n"
        "keywords: 3-6 个关键词数组\n"
        "summary: 120 字以内的教案摘要\n";

    Json::Value body;
    body["model"] = config_.model;
    body["temperature"] = 0.2;
    Json::Value messages(Json::arrayValue);

    Json::Value systemMessage;
    systemMessage["role"] = "system";
    systemMessage["content"] =
        "你是 K12 教案结构化解析助手，擅长提取教案元数据，"
        "并且严格输出合法 JSON。";
    messages.append(systemMessage);

    Json::Value userMessage;
    userMessage["role"] = "user";
    userMessage["content"] = prompt;
    messages.append(userMessage);
    body["messages"] = messages;

    auto request = drogon::HttpRequest::newHttpJsonRequest(body);
    request->setMethod(drogon::Post);
    request->setPath(config_.chatPath);
    request->addHeader("Authorization", "Bearer " + config_.apiKey);

    auto client = drogon::HttpClient::newHttpClient(config_.baseUrl);

    LOG_INFO << "real parse request: fileId=" << input.fileId
             << ", model=" << config_.model
             << ", baseUrl=" << config_.baseUrl
             << ", path=" << config_.chatPath;

    client->sendRequest(
        request,
        [callback, client, started, model = config_.model](
            drogon::ReqResult result,
            const drogon::HttpResponsePtr &response) {
            const int64_t cost = elapsedMs(started);

            if (result != drogon::ReqResult::Ok || response == nullptr)
            {
                auto output = ParseOutput::failure(
                    "调用真实大模型失败：网络请求未成功（result=" +
                        std::to_string(static_cast<int>(result)) + "）",
                    "real",
                    model);
                output.costMs = cost;
                callback(std::move(output));
                return;
            }

            if (response->statusCode() != drogon::k200OK)
            {
                std::string bodyText(response->body());
                if (bodyText.size() > 200)
                {
                    bodyText.resize(200);
                }
                auto output = ParseOutput::failure(
                    "调用真实大模型失败：HTTP " +
                        std::to_string(static_cast<int>(response->statusCode())) +
                        "，响应：" + bodyText,
                    "real",
                    model);
                output.costMs = cost;
                callback(std::move(output));
                return;
            }

            auto json = response->getJsonObject();
            if (!json)
            {
                auto output = ParseOutput::failure(
                    "真实大模型响应不是合法 JSON", "real", model);
                output.costMs = cost;
                callback(std::move(output));
                return;
            }

            const Json::Value &choices = (*json)["choices"];
            if (!choices.isArray() || choices.empty())
            {
                auto output =
                    ParseOutput::failure("真实大模型响应缺少 choices 字段", "real",
                                         model);
                output.costMs = cost;
                callback(std::move(output));
                return;
            }

            const std::string content =
                choices[0]["message"]["content"].asString();
            const std::string payload = stripCodeFence(content);

            Json::CharReaderBuilder builder;
            std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
            Json::Value parsed;
            std::string parseErrors;
            if (!reader->parse(payload.data(),
                               payload.data() + payload.size(),
                               &parsed,
                               &parseErrors))
            {
                auto output = ParseOutput::failure(
                    "真实大模型输出无法解析为 JSON：" + parseErrors, "real",
                    model);
                output.costMs = cost;
                callback(std::move(output));
                return;
            }

            ParseOutput output;
            output.modelType = "real";
            output.modelName = model;
            output.costMs = cost;
            output.lessonTitle =
                pickField(parsed, {"lesson_title", "lessonTitle", "title"});
            output.gradeSubject =
                pickField(parsed, {"grade_subject", "gradeSubject", "grade"});
            output.teachingObjectives = pickField(
                parsed,
                {"teaching_objectives", "teachingObjectives", "objectives"});
            output.keywords =
                joinKeywords(parsed.isMember("keywords") ? parsed["keywords"]
                                                         : parsed["keyword"]);
            output.summary =
                pickField(parsed, {"summary", "abstract", "description"});

            if (output.lessonTitle.empty() && output.summary.empty())
            {
                auto failure = ParseOutput::failure(
                    "真实大模型返回内容缺少必要字段（lesson_title/summary）",
                    "real",
                    model);
                failure.costMs = cost;
                callback(std::move(failure));
                return;
            }

            output.success = true;
            callback(std::move(output));
        });
}
