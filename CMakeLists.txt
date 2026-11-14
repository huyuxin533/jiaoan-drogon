cmake_minimum_required(VERSION 3.16)

project(ai_lesson_parse
    VERSION 1.0.0
    DESCRIPTION "AI 教育平台 - 教案上传与结构化解析服务"
    LANGUAGES CXX)

# ---------------------------------------------------------------------------
# 全局编译选项
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ---------------------------------------------------------------------------
# 依赖
#   Drogon 1.7.x（需在编译时开启 MySQL 支持，即 -DBUILD_MYSQL=ON）
#   MySQL / MariaDB 客户端、jsoncpp、zlib、uuid 等依赖由 Drogon 的 CMake 配置传递
# ---------------------------------------------------------------------------
find_package(Drogon REQUIRED)

# ---------------------------------------------------------------------------
# 源文件（按目录分层：common / config / model / dao / provider / service / controller）
# ---------------------------------------------------------------------------
file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc)

add_executable(lesson_server ${APP_SOURCES})

target_include_directories(lesson_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(lesson_server PRIVATE Drogon::Drogon)

target_compile_options(lesson_server PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
    -Wno-deprecated-declarations)

# 安装：二进制 + 运行期配置
install(TARGETS lesson_server RUNTIME DESTINATION bin)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/config.json DESTINATION etc/ai_lesson_parse)

message(STATUS "ai_lesson_parse: build type = ${CMAKE_BUILD_TYPE}")
