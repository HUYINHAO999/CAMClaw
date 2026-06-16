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
    src/AgentPlanDraft.cpp
    src/AgentPlanDraftJsonCodec.cpp
    src/AgentPlanExecutor.cpp
    src/AgentPlanExecutionResultJsonCodec.cpp
    src/AgentWorkflowService.cpp
    src/HumanInLoopService.cpp
    src/PlanDraftFactory.cpp
    src/SemanticIntentPlan.cpp
    src/SkillRuntime.cpp
    src/TargetResolver.cpp
    src/TraceService.cpp
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

add_executable(camclaw_agent_plan_draft_tests
    tests/test_agent_plan_draft.cpp
)

target_link_libraries(camclaw_agent_plan_draft_tests
    PRIVATE
        camclaw_core
)

add_test(NAME camclaw_agent_plan_draft_tests COMMAND camclaw_agent_plan_draft_tests)

add_executable(camclaw_skill_runtime_tests
    tests/test_skill_runtime.cpp
)

target_link_libraries(camclaw_skill_runtime_tests
    PRIVATE
        camclaw_core
)

add_test(NAME camclaw_skill_runtime_tests COMMAND camclaw_skill_runtime_tests)

add_executable(camclaw_action_gateway_tests
    tests/test_action_gateway.cpp
)

target_link_libraries(camclaw_action_gateway_tests
    PRIVATE
        camclaw_core
)

add_test(NAME camclaw_action_gateway_tests COMMAND camclaw_action_gateway_tests)

add_executable(camclaw_plan_draft_factory_tests
    tests/test_plan_draft_factory.cpp
)

target_link_libraries(camclaw_plan_draft_factory_tests
    PRIVATE
        camclaw_core
)

add_test(NAME camclaw_plan_draft_factory_tests COMMAND camclaw_plan_draft_factory_tests)

add_executable(camclaw_agent_plan_executor_tests
    tests/test_agent_plan_executor.cpp
)

target_link_libraries(camclaw_agent_plan_executor_tests
    PRIVATE
        camclaw_core
)

add_test(NAME camclaw_agent_plan_executor_tests COMMAND camclaw_agent_plan_executor_tests)

add_executable(camclaw_repository_tests
    tests/test_repository.cpp
)

target_link_libraries(camclaw_repository_tests
    PRIVATE
        camclaw_core
)

add_test(NAME camclaw_repository_tests COMMAND camclaw_repository_tests)

add_executable(camclaw_agent_plan_draft_json_codec_tests
    tests/test_agent_plan_draft_json_codec.cpp
)

target_link_libraries(camclaw_agent_plan_draft_json_codec_tests
    PRIVATE
        camclaw_core
)

add_test(NAME camclaw_agent_plan_draft_json_codec_tests COMMAND camclaw_agent_plan_draft_json_codec_tests)

add_executable(camclaw_python_backend_draft_to_cpp_execution_tests
    tests/test_python_backend_draft_to_cpp_execution.cpp
)

target_link_libraries(camclaw_python_backend_draft_to_cpp_execution_tests
    PRIVATE
        camclaw_core
)

add_test(NAME camclaw_python_backend_draft_to_cpp_execution_tests COMMAND camclaw_python_backend_draft_to_cpp_execution_tests)

add_executable(camclaw_agent_plan_execution_result_json_codec_tests
    tests/test_agent_plan_execution_result_json_codec.cpp
)

target_link_libraries(camclaw_agent_plan_execution_result_json_codec_tests
    PRIVATE
        camclaw_core
)

add_test(NAME camclaw_agent_plan_execution_result_json_codec_tests COMMAND camclaw_agent_plan_execution_result_json_codec_tests)

add_executable(camclaw_trace_service_tests
    tests/test_trace_service.cpp
)

target_link_libraries(camclaw_trace_service_tests
    PRIVATE
        camclaw_core
)

add_test(NAME camclaw_trace_service_tests COMMAND camclaw_trace_service_tests)

add_executable(camclaw_human_in_loop_tests
    tests/test_human_in_loop.cpp
)

target_link_libraries(camclaw_human_in_loop_tests
    PRIVATE
        camclaw_core
)

add_test(NAME camclaw_human_in_loop_tests COMMAND camclaw_human_in_loop_tests)

add_executable(camclaw_semantic_intent_tests
    tests/test_semantic_intent.cpp
)

target_link_libraries(camclaw_semantic_intent_tests
    PRIVATE
        camclaw_core
)

add_test(NAME camclaw_semantic_intent_tests COMMAND camclaw_semantic_intent_tests)

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
