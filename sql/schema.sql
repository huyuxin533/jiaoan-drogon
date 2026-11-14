-- =============================================================================
-- AI 教育平台 - 教案上传与结构化解析模块
-- 数据库：MySQL 8.0+ / MariaDB 10.5+
-- 字符集：utf8mb4
-- =============================================================================

CREATE DATABASE IF NOT EXISTS `ai_lesson`
    DEFAULT CHARACTER SET utf8mb4
    COLLATE utf8mb4_general_ci;

USE `ai_lesson`;

-- -----------------------------------------------------------------------------
-- 1. 教案文件表：保存上传文件的基础信息、本地存储路径与解析状态
--    状态流转：PENDING(待解析) -> PARSING(解析中) -> SUCCESS(解析成功) / FAILED(解析失败)
-- -----------------------------------------------------------------------------
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

-- -----------------------------------------------------------------------------
-- 2. 教案解析结果表：每次解析都会写入一条记录（保留历史），查询时取最新一条
--    parse_status = SUCCESS 时结构化字段有值；parse_status = FAILED 时 fail_reason 有值
-- -----------------------------------------------------------------------------
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

-- -----------------------------------------------------------------------------
-- 演示数据（可选，默认注释）
-- -----------------------------------------------------------------------------
-- INSERT INTO `lesson_file` (`file_id`,`original_name`,`file_type`,`file_size`,`storage_path`,`uploader`,`status`)
-- VALUES ('demo-file-id-0001','七年级数学-一元一次方程.pdf','PDF',204800,'2026/11/13/demo-file-id-0001.pdf','demo','PENDING');
