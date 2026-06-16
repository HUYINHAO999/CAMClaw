cmake_minimum_required(VERSION 3.10)

project(CAMClaw LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

if(MSVC)
    add_compile_options(/utf-8)
endif()

add_library(camclaw_core
    src/AgentPlanExecutionResultJsonCodec.cpp
    src/AgentWorkflowService.cpp
    src/BrowserConsole.cpp
    src/HumanInLoopService.cpp
    src/SemanticIntentPlan.cpp
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

add_executable(camclaw_repository_tests
    tests/test_repository.cpp
)

target_link_libraries(camclaw_repository_tests
    PRIVATE
        camclaw_core
)

add_test(NAME camclaw_repository_tests COMMAND camclaw_repository_tests)

add_executable(camclaw_agent_plan_execution_result_json_codec_tests
    tests/test_agent_plan_execution_result_json_codec.cpp
)

target_link_libraries(camclaw_agent_plan_execution_result_json_codec_tests
    PRIVATE
        camclaw_core
)

add_test(NAME camclaw_agent_plan_execution_result_json_codec_tests COMMAND camclaw_agent_plan_execution_result_json_codec_tests)

add_executable(camclaw_semantic_intent_tests
    tests/test_semantic_intent.cpp
)

target_link_libraries(camclaw_semantic_intent_tests
    PRIVATE
        camclaw_core
)

add_test(NAME camclaw_semantic_intent_tests COMMAND camclaw_semantic_intent_tests)

add_executable(camclaw_browser_console_tests
    tests/test_browser_console.cpp
)

target_link_libraries(camclaw_browser_console_tests
    PRIVATE
        camclaw_core
)

add_test(NAME camclaw_browser_console_tests COMMAND camclaw_browser_console_tests)

find_package(Qt6 COMPONENTS Widgets Network Test QUIET)

if(Qt6_FOUND)
    add_library(camclaw_qt_widgets
        qt/src/AgentReviewDialog.cpp
        qt/src/AgentDraftService.cpp
        qt/src/BrowserTree.cpp
        qt/src/CamMainWindow.cpp
        qt/src/CamViewport.cpp
        qt/src/OperationEditDialog.cpp
        qt/include/camclaw/qt/AgentReviewDialog.h
        qt/include/camclaw/qt/AgentDraftService.h
        qt/include/camclaw/qt/BrowserTree.h
        qt/include/camclaw/qt/CamMainWindow.h
        qt/include/camclaw/qt/CamViewport.h
        qt/include/camclaw/qt/OperationEditDialog.h
    )

    target_include_directories(camclaw_qt_widgets
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/qt/include
    )

    target_link_libraries(camclaw_qt_widgets
        PUBLIC
            camclaw_core
            Qt6::Widgets
            Qt6::Network
    )

    add_executable(camclaw_qt_app
        qt/src/main.cpp
    )

    target_link_libraries(camclaw_qt_app
        PRIVATE
            camclaw_qt_widgets
    )

    add_executable(camclaw_qt_tests
        qt/tests/test_cam_main_window.cpp
    )

    target_link_libraries(camclaw_qt_tests
        PRIVATE
            camclaw_qt_widgets
            Qt6::Test
    )

    add_test(NAME camclaw_qt_tests COMMAND camclaw_qt_tests)
    set_tests_properties(camclaw_qt_tests PROPERTIES
        ENVIRONMENT "PATH=${Qt6_DIR}/../../../bin;$ENV{PATH};QT_QPA_PLATFORM=windows"
    )
else()
    message(STATUS "Qt6 was not found; Qt app and Qt tests are disabled.")
endif()
