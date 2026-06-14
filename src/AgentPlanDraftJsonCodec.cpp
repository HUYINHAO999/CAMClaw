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

bool hasRequiredInput(const std::map<std::string, std::string>& inputs, const std::string& name)
{
    const std::map<std::string, std::string>::const_iterator found = inputs.find(name);
    return found != inputs.end() && !found->second.empty();
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
    std::string skill_id;
    if (!findJsonStringValueAfter(json, "trace_id", 0u, trace_id)
        || !findJsonStringValueAfter(json, "status", 0u, status)
        || !findJsonStringValueAfter(json, "skill_id", 0u, skill_id)) {
        result.error_code = "invalid_draft_json";
        result.message = "Backend draft JSON is missing required top-level draft fields.";
        return result;
    }

    if (status != "pending_review") {
        result.error_code = "unsupported_draft_status";
        result.message = "Only pending_review backend drafts can be decoded.";
        return result;
    }

    const std::size_t inputs_pos = json.find("\"inputs\"");
    if (inputs_pos == std::string::npos) {
        result.error_code = "invalid_draft_json";
        result.message = "Backend draft JSON is missing step inputs.";
        return result;
    }

    const char* required_inputs[] = {
        "target_object_id",
        "operation_type",
        "tool_id",
        "stepover",
        "stepdown",
        "tolerance"
    };

    std::map<std::string, std::string> inputs;
    for (std::size_t index = 0u; index < sizeof(required_inputs) / sizeof(required_inputs[0]); ++index) {
        std::string value;
        if (!findJsonStringValueAfter(json, required_inputs[index], inputs_pos, value)) {
            result.error_code = "missing_draft_input";
            result.message = std::string("Backend draft JSON is missing input: ") + required_inputs[index];
            return result;
        }
        inputs[required_inputs[index]] = value;
    }

    for (std::size_t index = 0u; index < sizeof(required_inputs) / sizeof(required_inputs[0]); ++index) {
        if (!hasRequiredInput(inputs, required_inputs[index])) {
            result.error_code = "missing_draft_input";
            result.message = std::string("Backend draft JSON has empty input: ") + required_inputs[index];
            return result;
        }
    }

    result.draft = AgentPlanDraft(trace_id);
    result.draft.addSkillStep(SkillStepDraft(skill_id, inputs));
    result.ok = true;
    return result;
}

} // namespace camclaw
