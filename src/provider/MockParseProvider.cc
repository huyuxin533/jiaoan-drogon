#include "provider/MockParseProvider.h"

#include <drogon/drogon.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <random>
#include <vector>

#include "common/StringUtil.h"

namespace
{
constexpr size_t kHashSampleBytes = 8192;

uint64_t fnv1a(const void *data, size_t length, uint64_t hash = 1469598103934665603ULL)
{
    const auto *bytes = static_cast<const unsigned char *>(data);
    for (size_t i = 0; i < length; ++i)
    {
        hash ^= bytes[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

std::mt19937 &randomEngine()
{
    thread_local std::mt19937 engine(std::random_device{}());
    return engine;
}

std::string readSample(const std::string &path)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
    {
        return "";
    }
    std::string buffer;
    buffer.resize(kHashSampleBytes);
    stream.read(&buffer[0], static_cast<std::streamsize>(buffer.size()));
    buffer.resize(static_cast<size_t>(stream.gcount()));
    return buffer;
}

int64_t elapsedMs(const std::chrono::steady_clock::time_point &start)
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - start)
        .count();
}

const std::vector<std::string> &gradeKeywords()
{
    static const std::vector<std::string> grades = {
        "一年级", "二年级", "三年级", "四年级", "五年级", "六年级",
        "七年级", "八年级", "九年级", "高一",   "高二",   "高三"};
    return grades;
}

const std::vector<std::string> &subjectKeywords()
{
    static const std::vector<std::string> subjects = {
        "语文", "数学",   "英语", "物理",   "化学",   "生物", "政治",
        "历史", "地理",   "科学", "信息技术", "音乐", "美术", "体育"};
    return subjects;
}

const std::vector<std::string> &objectiveTemplates()
{
    static const std::vector<std::string> templates = {
        "【知识与技能】能够准确复述“{topic}”的核心概念，并独立完成基础练习。",
        "【知识与技能】掌握“{topic}”的关键方法，能在新情境中正确应用。",
        "【过程与方法】通过小组合作与课堂探究，经历“{topic}”的完整探究过程。",
        "【过程与方法】借助对比、归纳与思维导图，梳理“{topic}”的知识结构。",
        "【情感态度】在“{topic}”的学习中体会学科价值，形成主动思考的习惯。",
        "【情感态度】通过真实情境任务，增强合作意识与表达自信。"};
    return templates;
}

const std::vector<std::string> &keywordBank()
{
    static const std::vector<std::string> keywords = {
        "核心素养", "课堂导入", "合作探究", "分层作业", "教学反思",
        "情境教学", "大单元设计", "学情分析", "板书设计", "课堂评价",
        "跨学科融合", "思维导图"};
    return keywords;
}

const std::vector<std::string> &failReasons()
{
    static const std::vector<std::string> reasons = {
        "模拟解析失败：文档结构无法识别，正文提取为空（mock 随机失败）",
        "模拟解析失败：模型上下文超时，未返回完整结构化结果（mock 随机失败）",
        "模拟解析失败：文档疑似扫描件，未检测到可解析文本层（mock 随机失败）",
        "模拟解析失败：模型输出 JSON 校验不通过（mock 随机失败）",
        "模拟解析失败：解析服务临时不可用，请稍后重试（mock 随机失败）"};
    return reasons;
}

std::string replaceTopic(const std::string &templ, const std::string &topic)
{
    const std::string placeholder = "{topic}";
    std::string result = templ;
    const auto pos = result.find(placeholder);
    if (pos != std::string::npos)
    {
        result.replace(pos, placeholder.size(), topic);
    }
    return result;
}
}  // namespace

MockParseProvider::MockParseProvider(MockConfig config) : config_(std::move(config))
{
}

void MockParseProvider::parse(const ParseInput &input, ParseCallback callback)
{
    const auto started = std::chrono::steady_clock::now();

    if (!std::filesystem::exists(input.storagePath))
    {
        callback(ParseOutput::failure("本地文件不存在，无法解析", modelType(),
                                      modelName()));
        return;
    }

    const std::string sample = readSample(input.storagePath);

    // 结果与文件强相关：文件名 + 大小 + 内容片段共同决定哈希种子
    uint64_t seed = fnv1a(input.originalName.data(), input.originalName.size());
    seed = fnv1a(&input.fileSize, sizeof(input.fileSize), seed);
    if (!sample.empty())
    {
        seed = fnv1a(sample.data(), sample.size(), seed);
    }

    // 随机成功/失败；文件名包含 fail_keyword 时强制失败（便于自测）
    const bool forcedFailure =
        !config_.failKeyword.empty() &&
        util::containsIgnoreCase(input.originalName, config_.failKeyword);
    std::uniform_real_distribution<double> probability(0.0, 1.0);
    const bool success =
        !forcedFailure && probability(randomEngine()) < config_.successRate;

    ParseOutput output;
    if (success)
    {
        output = buildOutput(input, seed);
    }
    else
    {
        const size_t reasonIndex =
            static_cast<size_t>(seed % failReasons().size());
        output = ParseOutput::failure(failReasons()[reasonIndex], modelType(),
                                      modelName());
    }

    const int minDelay = config_.minDelayMs;
    const int maxDelay = std::max(config_.minDelayMs, config_.maxDelayMs);
    std::uniform_int_distribution<int> delayDist(minDelay, maxDelay);
    const int delayMs = delayDist(randomEngine());

    LOG_INFO << "mock parse scheduled: fileId=" << input.fileId
             << ", name=" << input.originalName << ", delayMs=" << delayMs
             << ", expect=" << (success ? "success" : "failure");

    auto *loop = drogon::app().getLoop();
    loop->queueInLoop([loop, delayMs, callback, output, started]() mutable {
        loop->runAfter(static_cast<double>(delayMs) / 1000.0,
                       [callback, output, started]() mutable {
                           output.costMs = elapsedMs(started);
                           callback(std::move(output));
                       });
    });
}

ParseOutput MockParseProvider::buildOutput(const ParseInput &input,
                                           uint64_t seed) const
{
    const std::string stem =
        util::normalizeName(util::fileStem(input.originalName));
    const std::string topic = stem.empty() ? "本课主题" : stem;

    // 从文件名中识别年级与学科，识别不到时用哈希派生，保证不同文件结果不同
    std::string grade;
    for (const auto &candidate : gradeKeywords())
    {
        if (util::containsIgnoreCase(input.originalName, candidate))
        {
            grade = candidate;
            break;
        }
    }
    if (grade.empty())
    {
        grade = gradeKeywords()[static_cast<size_t>(seed %
                                                    gradeKeywords().size())];
    }

    std::string subject;
    for (const auto &candidate : subjectKeywords())
    {
        if (util::containsIgnoreCase(input.originalName, candidate))
        {
            subject = candidate;
            break;
        }
    }
    if (subject.empty())
    {
        subject = subjectKeywords()[static_cast<size_t>(
            (seed >> 8) % subjectKeywords().size())];
    }

    const std::string gradeSubject = grade + " / " + subject;

    ParseOutput output;
    output.success = true;
    output.modelType = modelType();
    output.modelName = modelName();

    if (util::containsIgnoreCase(stem, "教案") ||
        util::containsIgnoreCase(stem, "教学设计"))
    {
        output.lessonTitle = topic;
    }
    else
    {
        output.lessonTitle = topic + " 教学设计";
    }

    output.gradeSubject = gradeSubject;

    // 三个维度各取一条模板，模板由哈希选择，内容随文件变化
    std::string objectives;
    const auto &templates = objectiveTemplates();
    for (size_t dimension = 0; dimension < 3; ++dimension)
    {
        const size_t index =
            dimension * 2 + static_cast<size_t>((seed >> (dimension * 5)) % 2);
        if (!objectives.empty())
        {
            objectives += "\n";
        }
        objectives += replaceTopic(templates[index % templates.size()], topic);
    }
    output.teachingObjectives = objectives;

    // 关键词：学科 + 年级 + 主题 + 哈希挑选的 3 个教学法关键词
    std::vector<std::string> keywords{subject, grade, topic};
    const auto &bank = keywordBank();
    for (size_t i = 0; i < 3; ++i)
    {
        const std::string &candidate =
            bank[static_cast<size_t>((seed >> (i * 7 + 3)) % bank.size())];
        if (std::find(keywords.begin(), keywords.end(), candidate) ==
            keywords.end())
        {
            keywords.push_back(candidate);
        }
    }
    output.keywords = util::join(keywords, ",");

    output.summary =
        "本教案面向" + grade + subject +
        "教学，围绕“" + topic +
        "”设计完整课堂流程，包含情境导入、合作探究、分层练习与课堂评价四个环节。"
        "教学目标聚焦核心素养落地，重点关注学生的参与度与迁移应用能力，"
        "并结合学情提供差异化作业建议。";

    return output;
}
