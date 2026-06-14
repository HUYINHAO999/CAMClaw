#include "camclaw/AgentPlanDraft.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#define REQUIRE_TRUE(condition) \
    do { \
        if (!(condition)) { \
            std::cerr << "Requirement failed: " #condition << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            return EXIT_FAILURE; \
        } \
    } while (0)

#define REQUIRE_EQ(expected, actual) \
    do { \
        if (!((expected) == (actual))) { \
            std::cerr << "Requirement failed: " #expected " == " #actual << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            return EXIT_FAILURE; \
        } \
    } while (0)

static bool contains_event(const std::vector<std::string>& events, const std::string& event_name)
{
    return std::find(events.begin(), events.end(), event_name) != events.end();
}

static camclaw::AgentPlanDraft create_roughing_draft()
{
    std::map<std::string, std::string> inputs;
    inputs["target_object_id"] = "feature_001";
    inputs["operation_type"] = "roughing";
    inputs["tool_id"] = "tool_010";
    inputs["stepover"] = "2.0";
    inputs["stepdown"] = "1.0";
    inputs["tolerance"] = "0.02";

    camclaw::AgentPlanDraft draft("trace_draft_001");
    draft.addSkillStep(camclaw::SkillStepDraft("browser.create_roughing_operation_and_generate_toolpath", inputs));
    return draft;
}

static int user_can_edit_draft_input_values()
{
    camclaw::AgentPlanDraft draft = create_roughing_draft();

    const camclaw::DraftEditResult result = draft.editStepInput(0, "stepover", "1.5");

    REQUIRE_EQ(camclaw::DraftEditStatus::Edited, result.status);
    REQUIRE_EQ(std::string("browser.create_roughing_operation_and_generate_toolpath"), draft.stepAt(0).skillId());
    REQUIRE_EQ(std::string("1.5"), draft.stepAt(0).inputValue("stepover"));
    REQUIRE_EQ(1u, draft.stepCount());
    REQUIRE_TRUE(contains_event(draft.traceEvents(), "draft_input_edited"));

    return EXIT_SUCCESS;
}

static int user_cannot_create_new_inputs_by_editing()
{
    camclaw::AgentPlanDraft draft = create_roughing_draft();

    const camclaw::DraftEditResult result = draft.editStepInput(0, "internal_command_id", "browser.delete_operation");

    REQUIRE_EQ(camclaw::DraftEditStatus::InputNotFound, result.status);
    REQUIRE_TRUE(!draft.stepAt(0).hasInput("internal_command_id"));
    REQUIRE_EQ(1u, draft.stepCount());

    return EXIT_SUCCESS;
}

static int rejected_draft_records_reason_and_cannot_be_edited()
{
    camclaw::AgentPlanDraft draft = create_roughing_draft();

    draft.reject("只要粗加工，不要精加工");
    const camclaw::DraftEditResult result = draft.editStepInput(0, "stepover", "1.5");

    REQUIRE_EQ(camclaw::DraftStatus::Rejected, draft.status());
    REQUIRE_EQ(std::string("只要粗加工，不要精加工"), draft.rejectionReason());
    REQUIRE_EQ(camclaw::DraftEditStatus::DraftClosed, result.status);
    REQUIRE_EQ(std::string("2.0"), draft.stepAt(0).inputValue("stepover"));
    REQUIRE_TRUE(contains_event(draft.traceEvents(), "draft_rejected"));

    return EXIT_SUCCESS;
}

int main()
{
    int status = user_can_edit_draft_input_values();
    if (status != EXIT_SUCCESS) {
        return status;
    }

    status = user_cannot_create_new_inputs_by_editing();
    if (status != EXIT_SUCCESS) {
        return status;
    }

    status = rejected_draft_records_reason_and_cannot_be_edited();
    if (status != EXIT_SUCCESS) {
        return status;
    }

    return EXIT_SUCCESS;
}
