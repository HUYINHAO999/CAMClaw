#include "camclaw/AgentPlanDraftJsonCodec.h"

#include <map>

namespace camclaw {

namespace {

bool findJsonStringValueAfter(
    const std::string& json,
    const std::string& key,
    std::size_t start,
    std::string& value)
{
    const std::string needle = "\"" + key + "\"";
    const std::size_t key_pos = json.find(needle, start);
    if (key_pos == std::string::npos) {
        return false;
    }

    const std::size_t colon_pos = json.find(':', key_pos + needle.size());
    if (colon_pos == std::string::npos) {
        return false;
    }

    std::size_t quote_pos = json.find('"', colon_pos + 1u);
    if (quote_pos == std::string::npos) {
        return false;
    }

    ++quote_pos;
    value.clear();
    while (quote_pos < json.size()) {
        const char ch = json[quote_pos++];
        if (ch == '"') {
            return true;
        }

        if (ch == '\\' && quote_pos < json.size()) {
            value.push_back(json[quote_pos++]);
            continue;
        }

        value.push_back(ch);
    }

    return false;
}

bool findJsonStringValueBetween(
    const std::string& json,
    const std::string& key,
    std::size_t start,
    std::size_t end,
    std::string& value)
{
    const std::string needle = "\"" + key + "\"";
    const std::size_t key_pos = json.find(needle, start);
    if (key_pos == std::string::npos || key_pos >= end) {
        return false;
    }

    const std::size_t colon_pos = json.find(':', key_pos + needle.size());
    if (colon_pos == std::string::npos || colon_pos >= end) {
        return false;
    }

    std::size_t quote_pos = json.find('"', colon_pos + 1u);
    if (quote_pos == std::string::npos || quote_pos >= end) {
        return false;
    }

    ++quote_pos;
    value.clear();
    while (quote_pos < json.size() && quote_pos < end) {
        const char ch = json[quote_pos++];
        if (ch == '"') {
            return true;
        }

        if (ch == '\\' && quote_pos < json.size() && quote_pos < end) {
            value.push_back(json[quote_pos++]);
            continue;
        }

        value.push_back(ch);
    }

    return false;
}

bool hasRequiredInput(const std::map<std::string, std::string>& inputs, const std::string& name)
{
    const std::map<std::string, std::string>::const_iterator found = inputs.find(name);
    return found != inputs.end() && !found->second.empty();
}

std::size_t nextStepStart(const std::string& json, std::size_t after)
{
    const std::size_t next_command = json.find("\"command_id\"", after);
    const std::size_t next_skill = json.find("\"skill_id\"", after);
    if (next_command == std::string::npos) {
        return next_skill;
    }
    if (next_skill == std::string::npos) {
        return next_command;
    }
    return next_command < next_skill ? next_command : next_skill;
}

} // namespace

AgentPlanDraftDecodeResult::AgentPlanDraftDecodeResult()
    : ok(false),
      draft("")
{
}

AgentPlanDraftDecodeResult AgentPlanDraftJsonCodec::decodeBackendDraft(const std::string& json) const
{
    AgentPlanDraftDecodeResult result;

    std::string trace_id;
    std::string status;
    if (!findJsonStringValueAfter(json, "trace_id", 0u, trace_id)
        || !findJsonStringValueAfter(json, "status", 0u, status)) {
        result.error_code = "invalid_draft_json";
        result.message = "Backend draft JSON is missing required top-level draft fields.";
        return result;
    }

    if (status != "pending_review") {
        result.error_code = "unsupported_draft_status";
        result.message = "Only pending_review backend drafts can be decoded.";
        return result;
    }

    result.draft = AgentPlanDraft(trace_id);

    std::size_t step_pos = nextStepStart(json, 0u);
    if (step_pos == std::string::npos) {
        result.error_code = "invalid_draft_json";
        result.message = "Backend draft JSON is missing step command_id.";
        return result;
    }

    while (step_pos != std::string::npos) {
        const std::size_t step_end = nextStepStart(json, step_pos + 1u);
        const std::size_t bounded_end = step_end == std::string::npos ? json.size() : step_end;

        std::string skill_id;
        std::string command_id;
        std::string schema_id;
        if (!findJsonStringValueBetween(json, "command_id", step_pos, bounded_end, command_id)) {
            if (!findJsonStringValueBetween(json, "skill_id", step_pos, bounded_end, skill_id)) {
                result.error_code = "invalid_draft_json";
                result.message = "Backend draft JSON is missing step command_id.";
                return result;
            }
        }

        findJsonStringValueBetween(json, "schema_id", step_pos, bounded_end, schema_id);

        const std::size_t inputs_pos = json.find("\"inputs\"", step_pos);
        if (inputs_pos == std::string::npos || inputs_pos >= bounded_end) {
            result.error_code = "invalid_draft_json";
            result.message = "Backend draft JSON is missing step inputs.";
            return result;
        }

        std::map<std::string, std::string> inputs;
        const char* optional_inputs[] = {
            "target_object_id",
            "operation_type",
            "tool_id",
            "stepover",
            "stepdown",
            "tolerance",
            "batch_count",
            "auto_generate_toolpath",
            "visibility",
            "scope",
            "toolpath_ids",
            "parameter_name",
            "parameter_value",
            "recompute_toolpath"
        };

        for (std::size_t index = 0u; index < sizeof(optional_inputs) / sizeof(optional_inputs[0]); ++index) {
            std::string value;
            if (findJsonStringValueBetween(json, optional_inputs[index], inputs_pos, bounded_end, value)) {
                inputs[optional_inputs[index]] = value;
            }
        }

        if (!hasRequiredInput(inputs, "target_object_id")) {
            result.error_code = "missing_draft_input";
            result.message = "Backend draft JSON is missing input: target_object_id";
            return result;
        }

        if (!schema_id.empty()) {
            inputs["schema_id"] = schema_id;
        }
        result.draft.addSkillStep(SkillStepDraft(command_id.empty() ? skill_id : command_id, inputs));
        step_pos = step_end;
    }

    if (result.draft.stepCount() == 0u) {
        result.error_code = "invalid_draft_json";
        result.message = "Backend draft JSON is missing steps.";
        return result;
    }
    result.ok = true;
    return result;
}

} // namespace camclaw
