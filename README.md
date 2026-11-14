# AI 教案上传与结构化解析服务（C++ / Drogon / MySQL）

## 1. 项目目标与业务场景

### 1.1 业务背景

AI 教育平台需要让教师上传自己的教案文件（PDF / DOC / DOCX），由平台调用大模型把非结构化的教案文档解析成
结构化数据（教案标题、年级学科、教学目标、关键词、摘要），用于后续的**教案检索、智能推荐、备课助手和教研数据分析**。

本仓库实现该链路的**后端服务**（无前端、无 Redis / MQ），核心能力：

- 教案文件上传与本地文件存储（单文件最大 50MB，仅 PDF / DOC / DOCX）；
- 文件元数据持久化与分页查询；
- AI 解析任务触发与状态机流转：`待解析(PENDING) -> 解析中(PARSING) -> 解析成功(SUCCESS) / 解析失败(FAILED)`；
- 解析结果查询（未完成返回状态，失败返回失败原因）；
- AI 解析层可插拔：默认 **Mock 模式**（异步模拟耗时、随机成功失败），可切换 **真实大模型**（OpenAI 兼容协议）。

### 1.2 设计目标

| 目标 | 实现方式 |
| --- | --- |
| 分层清晰 | `controller -> service -> dao -> model`，AI 能力独立为 `provider` 层 |
| 可扩展 | `ParseProvider` 抽象基类 + 工厂，新增模型只加实现类 |
| 可观测 | 统一返回体、统一错误码、关键节点日志 |
| 可测试 | Mock 模式支持成功率/耗时/强制失败关键字配置 |
| 健壮性 | 参数校验、全局异常兜底、状态原子抢占、落库失败回滚文件 |

---

## 2. 技术栈

| 类别 | 选型 |
| --- | --- |
| 语言 | C++17 |
| Web 框架 | [Drogon](https://github.com/drogonframework/drogon) 1.7.4（异步非阻塞 HTTP） |
| 数据库 | MySQL 8.x（驱动使用 MariaDB Connector/C） |
| 数据访问 | Drogon ORM（`execSqlAsync` 异步接口） |
| 序列化 | jsoncpp |
| 文件存储 | 本地磁盘（按日期分目录，文件名使用 UUID） |
| 构建 | CMake 3.16+ / GCC 9.4 |
| 其他 | 无 Redis、无消息队列、无前端 |

运行环境（本项目自测环境）：Ubuntu 20.04、GCC 9.4.0、CMake 3.16.3、Drogon 1.7.4、MySQL 8.0。

---

## 3. 依赖安装、编译启动

### 3.1 安装系统依赖

```bash
sudo apt update
sudo apt install -y build-essential cmake git \
    libjsoncpp-dev uuid-dev zlib1g-dev libssl-dev libbrotli-dev \
    libc-ares-dev libmariadb-dev libmariadb-dev-compat
```

### 3.2 编译安装 Drogon（必须开启 MySQL 支持）

> 注意：如果安装发行版自带的 Drogon，很可能没有编译 ORM/MySQL 支持，启动时会报
> `No database is supported by drogon`。必须从源码编译并显式开启 `BUILD_MYSQL=ON`。

```bash
git clone https://github.com/drogonframework/drogon.git
cd drogon
git submodule update --init

mkdir build && cd build
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_MYSQL=ON \
  -DBUILD_POSTGRESQL=OFF \
  -DBUILD_SQLITE=OFF \
  -DBUILD_EXAMPLES=OFF \
  -DBUILD_CTL=OFF
make -j$(nproc)
sudo make install
sudo ldconfig
```

若安装到自定义前缀（无 root 权限时）：

```bash
cmake .. -DCMAKE_INSTALL_PREFIX=$HOME/drogon-local ...   # 其余参数同上
make -j$(nproc) && make install

# 编译本项目时指定前缀
cmake -S . -B build -DCMAKE_PREFIX_PATH=$HOME/drogon-local
```

### 3.3 初始化数据库

```bash
# 1) 建库建表（脚本内容见第 4 节）
mysql -uroot -p < sql/schema.sql

# 2) 按实际环境修改 config.json 的 db_clients
```

### 3.4 编译与启动

```bash
cd /path/to/jiaoan

# 创建日志 / 上传目录（Drogon 要求日志目录必须已存在）
mkdir -p logs uploads

# 编译
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# 启动（默认读取 ./config.json）
./build/bin/lesson_server -c ./config.json
```

启动成功后监听 `0.0.0.0:8080`，日志输出到 `logs/lesson_server.log`，上传文件写入 `uploads/YYYY/MM/DD/`。

### 3.5 配置说明（config.json）

```jsonc
{
  "listeners": [{ "address": "0.0.0.0", "port": 8080, "client_max_body_size": "64M" }],
  "db_clients": [{ "rdbms": "mysql", "host": "127.0.0.1", "port": 3306,
                   "dbname": "ai_lesson", "user": "root", "passwd": "123456",
                   "characterSet": "utf8mb4", "connectionNumber": 4 }],
  "custom_config": {
    "storage": {
      "root_dir": "./uploads",            // 本地存储根目录
      "max_file_size": 52428800,          // 单文件 50MB
      "max_body_size": 67108864,          // HTTP body 64MB（含 multipart 开销）
      "allowed_extensions": ["pdf", "doc", "docx"]
    },
    "parse": {
      "provider": "mock",                 // mock | real
      "mock": {
        "success_rate": 0.8,              // 随机成功率
        "min_delay_ms": 300,
        "max_delay_ms": 1500,
        "fail_keyword": "fail"            // 文件名含该词时强制失败，便于测试
      },
      "real": {
        "base_url": "https://api.openai.com",   // 不含路径
        "api_key": "",
        "model": "qwen-plus",
        "chat_path": "/v1/chat/completions",
        "max_excerpt_bytes": 4096
      }
    }
  }
}
```

### 3.6 切换到真实大模型

1. 修改 `config.json`：`parse.provider` 改为 `real`，填写 `parse.real.base_url` / `api_key` / `model` / `chat_path`；
2. 重启服务，解析时 `model_type` 会写为 `real`，`model_name` 写入实际模型名；
3. 若供应商协议不是 OpenAI 兼容格式，只需修改 `src/provider/RealParseProvider.cc` 的请求/响应解析，
   `Service` 层无需改动；
4. 未配置 `base_url` 或 `api_key` 时不会静默降级，而是把明确的失败原因写入解析结果，便于运维发现配置问题。

> 说明：`RealParseProvider` 已实现完整请求链路，但因缺少可用 API Key，本次未做真实调用验证；默认使用 Mock。

---

## 4. 完整建表 SQL

文件位置：`sql/schema.sql`。

```sql
CREATE DATABASE IF NOT EXISTS `ai_lesson`
    DEFAULT CHARACTER SET utf8mb4
    COLLATE utf8mb4_general_ci;

USE `ai_lesson`;

DROP TABLE IF EXISTS `lesson_parse_result`;
DROP TABLE IF EXISTS `lesson_file`;

CREATE TABLE `lesson_file`
(
    `id`            BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '自增主键',
    `file_id`       VARCHAR(64)     NOT NULL COMMENT '对外文件ID（UUID，接口均以此为准）',
    `original_name` VARCHAR(255)    NOT NULL COMMENT '上传时的原始文件名',
    `file_type`     VARCHAR(16)     NOT NULL COMMENT '文件类型：PDF / DOC / DOCX',
    `file_size`     BIGINT UNSIGNED NOT NULL COMMENT '文件大小（字节）',
    `storage_path`  VARCHAR(512)    NOT NULL COMMENT '本地存储相对路径，如 2026/11/13/xxx.pdf',
    `uploader`      VARCHAR(64)     NOT NULL COMMENT '上传人',
    `status`        VARCHAR(16)     NOT NULL DEFAULT 'PENDING' COMMENT '状态：PENDING/PARSING/SUCCESS/FAILED',
    `created_at`    DATETIME(3)     NOT NULL DEFAULT CURRENT_TIMESTAMP(3) COMMENT '创建时间',
    `updated_at`    DATETIME(3)     NOT NULL DEFAULT CURRENT_TIMESTAMP(3) ON UPDATE CURRENT_TIMESTAMP(3) COMMENT '更新时间',
    PRIMARY KEY (`id`),
    UNIQUE KEY `uk_file_id` (`file_id`),
    KEY `idx_status_created` (`status`, `created_at`),
    KEY `idx_uploader` (`uploader`)
) ENGINE = InnoDB
  DEFAULT CHARSET = utf8mb4
  COLLATE = utf8mb4_general_ci COMMENT ='教案文件基础信息';

CREATE TABLE `lesson_parse_result`
(
    `id`                  BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '自增主键',
    `file_id`             VARCHAR(64)     NOT NULL COMMENT '关联 lesson_file.file_id',
    `lesson_title`        VARCHAR(255)             DEFAULT NULL COMMENT '教案标题',
    `grade_subject`       VARCHAR(128)             DEFAULT NULL COMMENT '年级学科，如：七年级 / 数学',
    `teaching_objectives` TEXT                     DEFAULT NULL COMMENT '教学目标',
    `keywords`            VARCHAR(512)             DEFAULT NULL COMMENT '关键词，英文逗号分隔',
    `summary`             TEXT                     DEFAULT NULL COMMENT '教案摘要',
    `model_type`          VARCHAR(16)     NOT NULL DEFAULT 'mock' COMMENT '模型标识：mock / real',
    `model_name`          VARCHAR(64)              DEFAULT NULL COMMENT '模型名称，如 mock-v1、qwen-plus',
    `cost_ms`             INT UNSIGNED    NOT NULL DEFAULT 0 COMMENT '解析耗时（毫秒）',
    `parse_status`        VARCHAR(16)     NOT NULL DEFAULT 'SUCCESS' COMMENT '本次解析结果：SUCCESS / FAILED',
    `fail_reason`         VARCHAR(1024)            DEFAULT NULL COMMENT '失败原因（失败时写入）',
    `created_at`          DATETIME(3)     NOT NULL DEFAULT CURRENT_TIMESTAMP(3) COMMENT '创建时间',
    `updated_at`          DATETIME(3)     NOT NULL DEFAULT CURRENT_TIMESTAMP(3) ON UPDATE CURRENT_TIMESTAMP(3) COMMENT '更新时间',
    PRIMARY KEY (`id`),
    KEY `idx_file_id_id` (`file_id`, `id`),
    KEY `idx_parse_status` (`parse_status`)
) ENGINE = InnoDB
  DEFAULT CHARSET = utf8mb4
  COLLATE = utf8mb4_general_ci COMMENT ='教案AI结构化解析结果';
```

> 补充说明：
> - `lesson_parse_result` 采用“一次解析一条记录”的方式保留历史，查询时取 `id` 最大的一条；
> - `parse_status` 为在需求字段基础上增加的扩展字段，用于区分历史记录中的成功/失败，失败原因写在 `fail_reason`。

---

## 5. 接口文档

### 5.0 通用约定

- 服务地址：`http://127.0.0.1:8080`
- 统一返回体：

```json
{
  "code": 0,
  "message": "ok",
  "data": {},
  "timestamp": 1794575046572
}
```

- `code = 0` 表示成功；业务失败仍返回 HTTP 200，通过 `code` 区分；未匹配路由返回 HTTP 404，未捕获异常返回 HTTP 500。
- 业务错误码：

| code | 含义 |
| --- | --- |
| 0 | 成功 |
| 1001 | 参数校验失败 |
| 1002 | 文件格式不支持 |
| 1003 | 文件超过大小限制 |
| 1004 | 资源不存在 |
| 1005 | 状态冲突（重复触发解析等） |
| 1006 | 本地文件存储失败 |
| 1007 | 解析失败 |
| 5000 | 服务器内部错误 |

- 文件状态枚举：`PENDING`（待解析）、`PARSING`（解析中）、`SUCCESS`（解析成功）、`FAILED`（解析失败）。

---

### 5.1 上传文件

`POST /api/files/upload`

| 项目 | 说明 |
| --- | --- |
| Content-Type | `multipart/form-data` |
| form 字段 | `file`：必填，PDF / DOC / DOCX，最大 50MB |
| form 字段 | `uploader`：必填，上传人，1~64 个可见字符（也可放在 query 上） |

成功响应：

```json
{
  "code": 0,
  "message": "上传成功",
  "data": {
    "fileId": "7A00B88B64624BB9A7341E622604FE7D",
    "originalName": "七年级数学-一元一次方程.pdf",
    "fileType": "PDF",
    "fileSize": 133,
    "uploader": "zhangsan",
    "status": "PENDING",
    "statusText": "待解析",
    "createdAt": "2026-11-13 21:04:06.546",
    "updatedAt": "2026-11-13 21:04:06.546"
  },
  "timestamp": 1794575046572
}
```

失败示例（扩展名非法）：

```json
{
  "code": 1002,
  "message": "仅支持 PDF、DOC、DOCX 格式，当前文件扩展名为 txt",
  "data": {},
  "timestamp": 1794575046595
}
```

其他错误：`1001` 缺少文件/文件为空/uploader 非法；`1003` 超过 50MB；`1006` 本地落盘失败；`5000` 数据库写入失败（同时删除已落盘文件）。

---

### 5.2 文件列表

`GET /api/files`

| 参数 | 位置 | 必填 | 说明 |
| --- | --- | --- | --- |
| page | query | 否 | 页码，默认 1，最小 1 |
| pageSize | query | 否 | 每页条数，默认 10，最大 100 |
| status | query | 否 | `PENDING` / `PARSING` / `SUCCESS` / `FAILED` |
| uploader | query | 否 | 上传人精确匹配 |
| keyword | query | 否 | 原始文件名模糊匹配 |

请求示例：`GET /api/files?page=1&pageSize=10&status=SUCCESS&uploader=zhangsan&keyword=数学`

成功响应：

```json
{
  "code": 0,
  "message": "ok",
  "data": {
    "list": [
      {
        "fileId": "7A00B88B64624BB9A7341E622604FE7D",
        "originalName": "七年级数学-一元一次方程.pdf",
        "fileType": "PDF",
        "fileSize": 133,
        "uploader": "zhangsan",
        "status": "SUCCESS",
        "statusText": "解析成功",
        "createdAt": "2026-11-13 21:04:06.546",
        "updatedAt": "2026-11-13 21:04:11.130"
      }
    ],
    "total": 1,
    "page": 1,
    "pageSize": 10,
    "totalPages": 1
  },
  "timestamp": 1794575052809
}
```

参数非法示例：

```json
{ "code": 1001, "message": "page 必须为 1~1000000 之间的整数", "data": {}, "timestamp": 1794575058772 }
```

---

### 5.3 文件详情

`GET /api/files/{id}`

成功响应：

```json
{
  "code": 0,
  "message": "ok",
  "data": {
    "fileId": "7A00B88B64624BB9A7341E622604FE7D",
    "originalName": "七年级数学-一元一次方程.pdf",
    "fileType": "PDF",
    "fileSize": 133,
    "uploader": "zhangsan",
    "status": "SUCCESS",
    "statusText": "解析成功",
    "createdAt": "2026-11-13 21:04:06.546",
    "updatedAt": "2026-11-13 21:04:11.130"
  },
  "timestamp": 1794575052809
}
```

失败响应：`1001` fileId 格式非法；`1004` 文件不存在。

> 出于安全考虑，接口不返回服务器本地存储路径，路径只落在数据库中。

---

### 5.4 查询文件状态

`GET /api/files/{id}/status`

成功响应：

```json
{
  "code": 0,
  "message": "ok",
  "data": {
    "fileId": "7A00B88B64624BB9A7341E622604FE7D",
    "status": "PENDING",
    "statusText": "待解析",
    "updatedAt": "2026-11-13 21:04:06.546"
  },
  "timestamp": 1794575050692
}
```

---

### 5.5 触发 AI 解析

`POST /api/files/{id}/parse`

- 无请求体；
- 接口立即返回 `PARSING`，实际解析在后台异步执行；
- 状态流转：`PENDING / FAILED -> PARSING -> SUCCESS / FAILED`；
- 并发安全：使用 `UPDATE ... WHERE status IN ('PENDING','FAILED')` 原子抢占，重复/并发请求只会有一个成功；
- 已解析成功（`SUCCESS`）的文件不允许重复解析，如需重新解析请重新上传。

成功响应：

```json
{
  "code": 0,
  "message": "解析任务已提交",
  "data": {
    "fileId": "7A00B88B64624BB9A7341E622604FE7D",
    "status": "PARSING",
    "statusText": "解析中",
    "provider": "mock",
    "modelName": "mock-v1"
  },
  "timestamp": 1794575050716
}
```

状态冲突：

```json
{ "code": 1005, "message": "文件正在解析中，请勿重复触发", "data": {}, "timestamp": 1794575050767 }
```

其他错误：`1004` 文件不存在；`5000` 状态更新失败。

---

### 5.6 查询解析结果

`GET /api/files/{id}/parse-result`

- 未完成（`PENDING` / `PARSING`）：`finished=false`，`result=null`，`message` 提示当前状态；
- 成功：`status=SUCCESS`、`finished=true`、`success=true`，`result` 为结构化结果；
- 失败：`status=FAILED`、`finished=true`、`success=false`，`failReason` 为失败原因。

解析成功：

```json
{
  "code": 0,
  "message": "ok",
  "data": {
    "fileId": "7A00B88B64624BB9A7341E622604FE7D",
    "originalName": "七年级数学-一元一次方程.pdf",
    "status": "SUCCESS",
    "statusText": "解析成功",
    "finished": true,
    "success": true,
    "message": "解析成功",
    "failReason": "",
    "result": {
      "fileId": "7A00B88B64624BB9A7341E622604FE7D",
      "lessonTitle": "七年级数学 一元一次方程 教学设计",
      "gradeSubject": "七年级 / 数学",
      "teachingObjectives": "【知识与技能】掌握“七年级数学 一元一次方程”的关键方法，能在新情境中正确应用。\n【过程与方法】通过小组合作与课堂探究，经历“七年级数学 一元一次方程”的完整探究过程。\n【情感态度】在“七年级数学 一元一次方程”的学习中体会学科价值，形成主动思考的习惯。",
      "keywords": ["数学", "七年级", "七年级数学 一元一次方程", "教学反思", "核心素养", "课堂评价"],
      "summary": "本教案面向七年级数学教学，围绕“七年级数学 一元一次方程”设计完整课堂流程，包含情境导入、合作探究、分层练习与课堂评价四个环节。教学目标聚焦核心素养落地，重点关注学生的参与度与迁移应用能力，并结合学情提供差异化作业建议。",
      "modelType": "mock",
      "modelName": "mock-v1",
      "costMs": 403,
      "parseStatus": "SUCCESS",
      "createdAt": "2026-11-13 21:04:11.120",
      "updatedAt": "2026-11-13 21:04:11.120"
    }
  },
  "timestamp": 1794575052790
}
```

解析中：

```json
{
  "code": 0,
  "message": "ok",
  "data": {
    "fileId": "7A00B88B64624BB9A7341E622604FE7D",
    "originalName": "七年级数学-一元一次方程.pdf",
    "status": "PARSING",
    "statusText": "解析中",
    "finished": false,
    "success": false,
    "message": "解析中，请稍后重试",
    "failReason": "",
    "result": null
  },
  "timestamp": 1794575050743
}
```

解析失败：

```json
{
  "code": 0,
  "message": "ok",
  "data": {
    "fileId": "4D7316DBEB464972880B91C90E8E8103",
    "originalName": "fail_英语教学设计.pdf",
    "status": "FAILED",
    "statusText": "解析失败",
    "finished": true,
    "success": false,
    "message": "模拟解析失败：模型上下文超时，未返回完整结构化结果（mock 随机失败）",
    "failReason": "模拟解析失败：模型上下文超时，未返回完整结构化结果（mock 随机失败）",
    "result": {
      "fileId": "4D7316DBEB464972880B91C90E8E8103",
      "lessonTitle": "",
      "gradeSubject": "",
      "teachingObjectives": "",
      "keywords": [],
      "summary": "",
      "modelType": "mock",
      "modelName": "mock-v1",
      "costMs": 760,
      "parseStatus": "FAILED",
      "createdAt": "2026-11-13 21:04:17.462",
      "updatedAt": "2026-11-13 21:04:17.462"
    }
  },
  "timestamp": 1794575058731
}
```

---

## 6. 自测记录（真实执行结果）

环境：本机 `127.0.0.1:8080`，MySQL `ai_lesson`，`parse.provider=mock`，`success_rate=0.8`。
测试文件：`七年级数学-一元一次方程.pdf`（133B）、`教案说明.txt`、`fail_英语教学设计.pdf`（30B）。

### 6.1 正常上传（PDF，含中文文件名）

```bash
curl -s -X POST \
  -F "file=@/tmp/七年级数学-一元一次方程.pdf" \
  -F "uploader=zhangsan" \
  http://127.0.0.1:8080/api/files/upload
```

结果：`code=0`，返回 `fileId=7A00B88B64624BB9A7341E622604FE7D`、`status=PENDING`，文件落盘
`uploads/2026/11/13/7A00B88B64624BB9A7341E622604FE7D.pdf`，数据库中 `original_name` 保留中文原名：

```
file_id                            original_name                file_type  file_size  uploader  status   storage_path
7A00B88B64624BB9A7341E622604FE7D   七年级数学-一元一次方程.pdf   PDF        133        zhangsan  SUCCESS  2026/11/13/7A00B88B64624BB9A7341E622604FE7D.pdf
```

### 6.2 非法格式上传（.txt）

```bash
curl -s -X POST -F "file=@/tmp/教案说明.txt" -F "uploader=zhangsan" \
  http://127.0.0.1:8080/api/files/upload
```

结果：被拒绝，`code=1002`，未产生文件与数据库记录：

```json
{"code":1002,"data":{},"message":"仅支持 PDF、DOC、DOCX 格式，当前文件扩展名为 txt","timestamp":1794575046595}
```

补充：再上传 52MB 的 `big_教案.pdf`，返回 `code=1003`、`message="文件大小超过限制（最大 50MB）"`，
说明大小校验生效且请求体上限（64MB）配置正确。

### 6.3 解析成功 / 解析失败

解析成功（同一文件先触发解析，1 秒内为 PARSING，约 2 秒后查询为 SUCCESS）：

```bash
curl -s -X POST http://127.0.0.1:8080/api/files/7A00B88B64624BB9A7341E622604FE7D/parse
# -> {"code":0,"message":"解析任务已提交","data":{"status":"PARSING",...}}

curl -s http://127.0.0.1:8080/api/files/7A00B88B64624BB9A7341E622604FE7D/parse-result
# -> data.status=SUCCESS, data.result.lessonTitle="七年级数学 一元一次方程 教学设计",
#    data.result.gradeSubject="七年级 / 数学", data.result.modelType="mock", data.result.costMs=403
```

解析失败（Mock 支持按文件名关键字 `fail` 强制失败，便于稳定复现）：

```bash
curl -s -X POST -F "file=@/tmp/fail_英语教学设计.pdf" -F "uploader=lisi" http://127.0.0.1:8080/api/files/upload
curl -s -X POST http://127.0.0.1:8080/api/files/4D7316DBEB464972880B91C90E8E8103/parse
sleep 2
curl -s http://127.0.0.1:8080/api/files/4D7316DBEB464972880B91C90E8E8103/parse-result
```

结果：`data.status=FAILED`、`data.success=false`、`data.failReason="模拟解析失败：模型上下文超时，未返回完整结构化结果（mock 随机失败）"`，
数据库 `lesson_parse_result.parse_status=FAILED` 且 `cost_ms=760`。

同时验证了：

- 解析中重复触发返回 `1005`；
- `status=FAILED&uploader=lisi&keyword=英语` 过滤列表返回 1 条；
- `page=0` / `status=XX` 返回 `1001`；
- 不存在的 fileId 返回 `1004`；未知路由返回 HTTP 404 + `1004`。

---

## 7. AI 工具使用说明

### 7.1 AI 完成的内容

- 工程骨架：`CMakeLists.txt`、目录分层、`config.json`、`sql/schema.sql`；
- 公共组件：统一返回封装 `ApiResponse`、业务异常 `BizException`、错误码 `ApiCode`、
  状态枚举 `FileStatus/ParseStatus`、字符串/时间工具、Controller 异常兜底；
- 数据层：实体模型、`RowMapper`、`LessonFileDao` / `LessonParseResultDao`（异步 ORM、分页、模糊查询、状态原子抢占）；
- AI 解析层：`ParseProvider` 抽象、`MockParseProvider`（异步定时器 + 随机成败 + 基于文件哈希派生结果）、
  `RealParseProvider`（OpenAI 兼容协议实现）、`ParseProviderFactory`；
- 业务层与接口层：上传落盘/校验、状态机流转、解析结果回写、6 个接口的 Controller 与参数校验；
- README 初稿与接口示例。

### 7.2 由我核对、修改的内容

- **业务规则**：确认“解析成功后不再允许重复解析”“失败可重试”“失败也写入一条结果记录”等规则；
- **数据库设计**：字段长度、`utf8mb4`、索引（status/uploader、file_id+id）、
  额外增加的 `parse_status` 字段与 `model_name` 字段；
- **Drogon 使用细节**：`setClientMaxMemoryBodySize` 才能让 multipart 整体驻留内存、
  日志目录必须预先创建、Drogon 必须编译 MySQL 支持（AI 初稿未覆盖这些运行期问题）；
- **异步回调安全**：修正 Controller 中 `std::move(callback)` 的捕获方式，
  避免同步校验异常时回调已被移动导致二次响应；
- **Mock 策略**：成功率 0.8、耗时 300~1500ms、`fail` 关键字强制失败，便于自测；
- **接口契约**：统一返回体字段、错误码取值、失败时仍以 `code=0` 返回状态与失败原因的约定；
- **自测与排错**：所有 curl 命令、返回 JSON、数据库查询结果均为本人实际执行并核对；
- **真实模型接入**：确认 API Key 只放配置/环境变量、不落库；提醒真实接入前需要补充 PDF/DOCX 文本抽取组件。

---

## 8. 排错记录

### 8.1 启动时报 `No database is supported by drogon`

- **现象**：服务启动即退出：

  ```
  FATAL No database is supported by drogon, please install the database development library first.
  ```

- **定位过程**：
  1. `config.json` 的 `db_clients` 已配置，但 Drogon 在加载配置阶段就失败，说明不是连接参数问题；
  2. 执行 `grep USE_MYSQL /usr/local/include/drogon/config.h`，结果为 `#define USE_MYSQL 0`；
  3. 确认系统里的 Drogon 虽然安装了 ORM 头文件，但构建时 `find_package(MySQL)` 未通过（缺少 MariaDB 开发库），
     实际没有编译 MySQL 驱动，`libdrogon.a` 中也没有 `MysqlConnection` 符号。
- **解决办法**：
  1. 安装 `libmariadb-dev libmariadb-dev-compat`；
  2. 重新编译 Drogon：`cmake .. -DBUILD_MYSQL=ON -DBUILD_POSTGRESQL=OFF -DBUILD_SQLITE=OFF`，安装到可用前缀；
  3. 本项目编译时指定 `-DCMAKE_PREFIX_PATH=<drogon 安装前缀>`，确认 `link.txt` 链接的是新编译的 `libdrogon.a`；
  4. 重启服务后正常连接 MySQL。

### 8.2 文件列表接口报 SQL 语法错误（`near '?)'`）

- **现象**：`GET /api/files` 返回 `code=5000`，日志：

  ```
  Error(1064) [42000] "You have an error in your SQL syntax ... near '?)'"
  sql:SELECT COUNT(*) AS total FROM lesson_file WHERE ('' = '' OR status = '')
      AND ('' = '' OR uploader = '') AND ('%%' = '' OR original_name LIKE ?)
  ```

- **定位过程**：
  1. 日志里 Drogon 已把参数内联到 SQL，最后一个 `?` 没被替换，说明**占位符数量多于传入参数数量**；
  2. 过滤条件为“参数为空即不过滤”的固定 SQL，每个条件有 2 个 `?`（判空 + 比较），共 6 个；
  3. 调用 `execSqlAsync` 时只传了 `status, status, uploader, uploader, likePattern` 5 个参数，漏了 `likePattern` 的判空参数；
  4. list 查询同样漏传，导致 `LIMIT ?` 之前就报错。
- **解决办法**：为 count / list 两条 SQL 各补齐 `likePattern` 参数（共 6 个和 8 个），
  并在自测中覆盖“无过滤 / 状态+上传人+关键字过滤 / 非法参数”三种情况，确认返回正确。

---

## 9. 目录结构

```
jiaoan/
├── CMakeLists.txt
├── config.json                 # 监听、数据库、存储、解析器配置
├── README.md
├── sql/
│   └── schema.sql              # 建库建表脚本
└── src/
    ├── main.cc                 # 入口：加载配置、请求体限制、全局异常、启动
    ├── common/                 # 返回封装、异常、错误码、状态枚举、工具
    │   ├── ApiCode.h
    │   ├── ApiGuard.h
    │   ├── ApiResponse.h
    │   ├── BizException.h
    │   ├── FileStatus.h/.cc
    │   ├── ServiceResult.h
    │   └── StringUtil.h/.cc
    ├── config/
    │   └── AppConfig.h/.cc     # custom_config 读取与校验
    ├── controller/
    │   └── FileController.h/.cc# 6 个 HTTP 接口
    ├── dao/
    │   ├── LessonFileDao.h/.cc
    │   ├── LessonParseResultDao.h/.cc
    │   └── RowMapper.h/.cc
    ├── model/
    │   ├── LessonFile.h/.cc
    │   └── LessonParseResult.h/.cc
    ├── provider/
    │   ├── ParseProvider.h     # 抽象基类
    │   ├── ParseTypes.h
    │   ├── MockParseProvider.h/.cc
    │   ├── RealParseProvider.h/.cc
    │   └── ParseProviderFactory.h/.cc
    └── service/
        ├── FileService.h/.cc
        └── ParseService.h/.cc
```

---

## 10. 已知边界与后续可优化项

1. 当前 multipart 请求体整体驻留内存（最大 64MB），生产环境建议改为流式落盘 + 边接收边写文件；
2. 本地文件存储未做磁盘配额、去重与清理策略；
3. 未实现鉴权（上传人由请求参数传入），接入平台时应从登录态获取；
4. 真实大模型接入需要补充 PDF/DOCX 正文抽取（当前仅传文件名、类型、大小和文本片段），
   并建议增加调用超时、重试与限流；
5. 无解析任务队列，服务重启时 `PARSING` 状态的文件会被保留，可增加启动自检把超时的 `PARSING` 重置为 `FAILED`。
