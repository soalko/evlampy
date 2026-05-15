cmake_minimum_required(VERSION 3.14)

include(FetchContent)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

function(add_evlampy_unit_test test_name)
    add_executable(${test_name} ${ARGN})
    target_link_libraries(${test_name}
        PRIVATE
            messenger_core
            GTest::gtest_main
    )
    target_include_directories(${test_name}
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}
    )
    add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()

add_evlampy_unit_test(test_message
    unit/chat/MessageTests.cpp
)

add_evlampy_unit_test(test_message_protocol
    unit/network/MessageProtocolTests.cpp
)

add_evlampy_unit_test(test_tcp_server
    unit/network/TcpServerTests.cpp
)

add_evlampy_unit_test(test_tcp_client
    unit/network/TcpClientTests.cpp
)

add_evlampy_unit_test(test_encryption_aes
    unit/encryption/AES256GCMStrategyTests.cpp
)

add_evlampy_unit_test(test_encryption_key_factory
    unit/encryption/OpenSSLKeyFactoryTests.cpp
)

add_evlampy_unit_test(test_chat_server
    unit/chat/ChatServerTests.cpp
)

add_evlampy_unit_test(test_chat_client
    unit/chat/ChatClientTests.cpp
)

add_evlampy_unit_test(test_patterns_observer
    unit/patterns/ObserverTests.cpp
)

add_evlampy_unit_test(test_patterns_command_logger
    unit/patterns/CommandLoggerTests.cpp
)

function(add_evlampy_scenario scenario_name source_file)
    add_executable(${scenario_name} ${source_file})
    target_link_libraries(${scenario_name} PRIVATE messenger_core)
    target_include_directories(${scenario_name}
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}
    )
    add_test(NAME ${scenario_name} COMMAND ${scenario_name})
endfunction()

add_evlampy_scenario(scenario_auth_flow scenarios/scenario_auth_flow.cpp)
add_evlampy_scenario(scenario_direct_message scenarios/scenario_direct_message.cpp)
add_evlampy_scenario(scenario_group_flow scenarios/scenario_group_flow.cpp)
add_evlampy_scenario(scenario_delete_for_all scenarios/scenario_delete_for_all.cpp)
add_evlampy_scenario(scenario_admin_actions scenarios/scenario_admin_actions.cpp)
add_evlampy_scenario(scenario_persistence_relogin scenarios/scenario_persistence_relogin.cpp)

