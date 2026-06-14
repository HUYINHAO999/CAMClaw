cmake_minimum_required(VERSION 3.10)

project(CAMClaw LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(camclaw_core
    src/AgentWorkflowService.cpp
)

target_include_directories(camclaw_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

enable_testing()

add_executable(camclaw_core_tests
    tests/test_agent_workflow.cpp
)

target_link_libraries(camclaw_core_tests
    PRIVATE
        camclaw_core
)

add_test(NAME camclaw_core_tests COMMAND camclaw_core_tests)
