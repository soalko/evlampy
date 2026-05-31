cmake_minimum_required(VERSION 3.10)
project(EvlampyMessenger VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

option(BUILD_TESTS "Build unit and scenario tests" OFF)
option(BUILD_GUI "Build minimal GUI executable" OFF)

if(APPLE AND NOT DEFINED OPENSSL_ROOT_DIR)
    execute_process(
        COMMAND brew --prefix openssl@3
        OUTPUT_VARIABLE _brew_openssl_prefix
        RESULT_VARIABLE _brew_openssl_result
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(_brew_openssl_result EQUAL 0 AND EXISTS "${_brew_openssl_prefix}")
        set(OPENSSL_ROOT_DIR "${_brew_openssl_prefix}")
        list(APPEND CMAKE_PREFIX_PATH "${_brew_openssl_prefix}")
    endif()
endif()

find_package(OpenSSL QUIET COMPONENTS Crypto)

if(NOT OpenSSL_FOUND)
    if(APPLE AND EXISTS "/opt/homebrew/opt/openssl@3/include/openssl/evp.h" AND EXISTS "/opt/homebrew/opt/openssl@3/lib/libcrypto.dylib")
        set(OPENSSL_INCLUDE_DIR "/opt/homebrew/opt/openssl@3/include")
        set(OPENSSL_CRYPTO_LIBRARY "/opt/homebrew/opt/openssl@3/lib/libcrypto.dylib")
        if(NOT TARGET OpenSSL::Crypto)
            add_library(OpenSSL::Crypto UNKNOWN IMPORTED)
            set_target_properties(OpenSSL::Crypto PROPERTIES
                IMPORTED_LOCATION "${OPENSSL_CRYPTO_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIR}"
            )
        endif()
        set(OpenSSL_FOUND TRUE)
    endif()
endif()

if(NOT OpenSSL_FOUND)
    message(FATAL_ERROR "OpenSSL (Crypto) not found. Install it and/or pass -DOPENSSL_ROOT_DIR=/path/to/openssl")
endif()

find_package(Threads REQUIRED)

set(CORE_SOURCES
        src/chat/ChatClient.cpp
        src/chat/ChatServer.cpp
        src/chat/Message.cpp

        src/network/MessageProtocol.cpp
        src/network/TcpClient.cpp
        src/network/TcpServer.cpp

        src/encryption/AES256GCMStrategy.cpp
        src/encryption/OpenSSLKeyFactory.cpp

        src/patterns/Command.cpp
        src/patterns/Logger.cpp
)

add_library(messenger_core ${CORE_SOURCES})

target_include_directories(messenger_core
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
)

if(TARGET OpenSSL::Crypto)
    target_link_libraries(messenger_core PRIVATE OpenSSL::Crypto)
else()
    # Fallback for older FindOpenSSL.cmake modules without imported targets.
    target_link_libraries(messenger_core PRIVATE ${OPENSSL_CRYPTO_LIBRARY})
    if(DEFINED OPENSSL_INCLUDE_DIR)
        target_include_directories(messenger_core PRIVATE ${OPENSSL_INCLUDE_DIR})
    endif()
endif()

target_link_libraries(messenger_core PRIVATE Threads::Threads)

add_executable(target_exec src/chat/main.cpp)
target_link_libraries(target_exec PRIVATE messenger_core)

target_include_directories(target_exec
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

if(BUILD_GUI)
  if(APPLE)
    enable_language(OBJCXX)
    add_executable(messenger_gui src/gui/main_gui.mm)
    target_link_libraries(messenger_gui PRIVATE messenger_core "-framework Cocoa")
    target_include_directories(messenger_gui PRIVATE ${CMAKE_SOURCE_DIR}/include)
  else()
    find_package(Qt6 COMPONENTS Widgets REQUIRED)
    add_executable(messenger_gui src/gui/main_gui.cpp)
    target_link_libraries(messenger_gui PRIVATE messenger_core Qt6::Widgets)
    target_include_directories(messenger_gui PRIVATE ${CMAKE_SOURCE_DIR}/include)
  endif()
endif()

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
