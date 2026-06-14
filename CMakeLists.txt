cmake_minimum_required(VERSION 3.10)

project(CAMClaw LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(camclaw_core
    src/AgentPlanDraft.cpp
    src/AgentPlanExecutor.cpp
    src/AgentPlanner.cpp
    src/AgentWorkflowService.cpp
    src/OpenAiCompatibleLlmClient.cpp
    src/PlanDraftFactory.cpp
    src/SkillRuntime.cpp
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

add_executable(camclaw_agent_planner_tests
    tests/test_agent_planner.cpp
)

target_link_libraries(camclaw_agent_planner_tests
    PRIVATE
        camclaw_core
)

add_test(NAME camclaw_agent_planner_tests COMMAND camclaw_agent_planner_tests)

add_executable(camclaw_agent_end_to_end_tests
    tests/test_agent_end_to_end.cpp
)

target_link_libraries(camclaw_agent_end_to_end_tests
    PRIVATE
        camclaw_core
)

add_test(NAME camclaw_agent_end_to_end_tests COMMAND camclaw_agent_end_to_end_tests)

add_executable(camclaw_openai_compatible_llm_client_tests
    tests/test_openai_compatible_llm_client.cpp
)

target_link_libraries(camclaw_openai_compatible_llm_client_tests
    PRIVATE
        camclaw_core
)

add_test(NAME camclaw_openai_compatible_llm_client_tests COMMAND camclaw_openai_compatible_llm_client_tests)
