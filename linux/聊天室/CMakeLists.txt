cmake_minimum_required(VERSION 3.15)
project(ChatSystem VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -g -O2)

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(MYSQL REQUIRED mysqlclient)
pkg_check_modules(HIREDIS REQUIRED hiredis)
pkg_check_modules(SPDLOG spdlog)
pkg_check_modules(CURL REQUIRED libcurl)
add_executable(chat_server
    server/main.cpp
    server/server.cpp
    server/reactor.cpp
    server/connection.cpp
    server/logger.cpp
    common/crypto.cpp
    common/config.cpp
    database/mysql_pool.cpp
    common/email.cpp
    database/redis_pool.cpp
)

target_include_directories(chat_server PRIVATE 
    include
    ${MYSQL_INCLUDE_DIRS}
    ${HIREDIS_INCLUDE_DIRS}
    ${CURL_INCLUDE_DIRS}
)

# spdlog
if(SPDLOG_FOUND)
    target_include_directories(chat_server PRIVATE ${SPDLOG_INCLUDE_DIRS})
elseif(EXISTS "${CMAKE_SOURCE_DIR}/third_party/spdlog/include")
    target_include_directories(chat_server PRIVATE "${CMAKE_SOURCE_DIR}/third_party/spdlog/include")
else()
    message(WARNING "spdlog not found")
endif()

target_link_libraries(chat_server
    OpenSSL::SSL 
    OpenSSL::Crypto
    ${MYSQL_LIBRARIES}
    ${HIREDIS_LIBRARIES}
    ${CURL_LIBRARIES}
    Threads::Threads
    curl
)

if(SPDLOG_FOUND)
    target_link_libraries(chat_server ${SPDLOG_LIBRARIES})
endif()

add_executable(chat_client
    client/main.cpp
    client/client.cpp
    common/crypto.cpp
)

target_include_directories(chat_client PRIVATE include)
target_link_libraries(chat_client
    OpenSSL::SSL OpenSSL::Crypto
    Threads::Threads
)