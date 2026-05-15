cmake_minimum_required(VERSION 3.10)
project(EvlampyMessenger VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

set(PROJECT_SOURCES
        build/src/chat/main.cpp
        build/src/chat/ChatClient.cpp
        build/src/chat/ChatServer.cpp
        build/src/chat/Message.cpp

        build/src/network/MessageProtocol.cpp
        build/src/network/TcpClient.cpp
        build/src/network/TcpServer.cpp

        build/src/encryption/AES256GCMStrategy.cpp
        build/src/encryption/OpenSSLKeyFactory.cpp

        build/src/patterns/Command.cpp
        build/src/patterns/Logger.cpp
)

add_executable(target_exec)
target_sources(target_exec PRIVATE ${PROJECT_SOURCES})

target_include_directories(target_exec
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(target_exec
    PRIVATE
        OpenSSL::Crypto
        Threads::Threads
)
