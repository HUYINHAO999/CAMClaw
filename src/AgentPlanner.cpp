#include "camclaw/AgentPlanner.h"

#include <cstdlib>
#include <map>

namespace camclaw {

namespace {

std::string envValue(const char* name)
{
    const char* value = std::getenv(name);
    if (value == 0) {
        return std::string();
    }

    return std::string(value);
}

void skipWhitespace(const std::string& text, std::size_t& index)
{
    while (index < text.size()
        && (text[index] == ' ' || text[index] == '\n' || text[index] == '\r' || text[index] == '\t')) {
        ++index;
    }
}

bool parseJsonString(const std::string& text, std::size_t& index, std::string& value)
{
    skipWhitespace(text, index);
    if (index >= text.size() || text[index] != '"') {
        return false;
    }

    ++index;
    value.clear();
    while (index < text.size()) {
        const char ch = text[index++];
        if (ch == '"') {
            return true;
        }

        if (ch == '\\') {
            if (index >= text.size()) {
                return false;
            }
            value.push_back(text[index++]);
            continue;
        }

        value.push_back(ch);
    }

    return false;
}

bool parseFlatStringObject(const std::string& json, std::map<std::string, std::string>& fields)
{
    std::size_t index = 0u;
    skipWhitespace(json, index);
    if (index >= json.size() || json[index] != '{') {
        return false;
    }

    ++index;
    skipWhitespace(json, index);
    if (index < json.size() && json[index] == '}') {
        return true;
    }

    while (index < json.size()) {
        std::string key;
        std::string value;
        if (!parseJsonString(json, index, key)) {
            return false;
        }

        skipWhitespace(json, index);
        if (index >= json.size() || json[index] != ':') {
            return false;
        }
        ++index;

        if (!parseJsonString(json, index, value)) {
            return false;
        }

        fields[key] = value;

        skipWhitespace(json, index);
        if (index < json.size() && json[index] == ',') {
            ++index;
            continue;
        }

        if (index < json.size() && json[index] == '}') {
            ++index;
            skipWhitespace(json, index);
            return index == json.size();
        }

        return false;
    }

    return false;
}

bool requireField(
    const std::map<std::string, std::string>& fields,
    const std::string& name,
    std::string& value)
{
    const std::map<std::string, std::string>::const_iterator found = fields.find(name);
    if (found == fields.end() || found->second.empty()) {
        return false;
    }

    value = found->second;
    return true;
}

} // namespace

LlmEndpointConfig LlmEndpointConfig::fromEnvironment()
{
    LlmEndpointConfig config;
    config.base_url = envValue("CAMCLAW_LLM_BASE_URL");
    config.api_key = envValue("CAMCLAW_LLM_API_KEY");
    config.model = envValue("CAMCLAW_LLM_MODEL");
    return config;
}

std::string LlmPlanningContract::roughingPlanJsonContract()
{
    return std::string()
        + "Return only a flat JSON object with string fields: "
        + "operation_type, tool_id, stepover, stepdown, tolerance. "
        + "Do not execute CAM commands. Do not include prose.";
}

AgentPlannerResult::AgentPlannerResult()
    : ok(false),
      draft("")
{
}

AgentPlanner::AgentPlanner(LlmClient& llm_client, const PlanDraftFactory& draft_factory)
    : llm_client_(llm_client),
      draft_factory_(draft_factory)
{
}

AgentPlannerResult AgentPlanner::createDraft(
    const std::string& trace_id,
    const std::string& user_request,
    const std::string& target_object_id,
    const PlanningContextProvider* context_provider) const
{
    AgentPlannerResult result;

    LlmPlanRequest request;
    request.trace_id = trace_id;
    request.user_request = user_request;
    request.target_object_id = target_object_id;
    request.response_contract = LlmPlanningContract::roughingPlanJsonContract();

    const std::string json = llm_client_.createPlanJson(request);

    RoughingPlanProposal proposal;
    std::string error_code;
    std::string message;
    if (!parseProposalJson(json, trace_id, target_object_id, proposal, error_code, message)) {
        result.ok = false;
        result.error_code = error_code;
        result.message = message;
        return result;
    }

    result.ok = true;
    result.draft = draft_factory_.createRoughingDraft(proposal, context_provider);
    return result;
}

bool AgentPlanner::parseProposalJson(
    const std::string& json,
    const std::string& trace_id,
    const std::string& target_object_id,
    RoughingPlanProposal& proposal,
    std::string& error_code,
    std::string& message) const
{
    std::map<std::string, std::string> fields;
    if (!parseFlatStringObject(json, fields)) {
        error_code = "invalid_llm_response";
        message = "LLM response must be a flat JSON object with string fields.";
        return false;
    }

    std::string operation_type;
    std::string tool_id;
    std::string stepover;
    std::string stepdown;
    std::string tolerance;

    if (!requireField(fields, "operation_type", operation_type)
        || !requireField(fields, "tool_id", tool_id)
        || !requireField(fields, "stepover", stepover)
        || !requireField(fields, "stepdown", stepdown)
        || !requireField(fields, "tolerance", tolerance)) {
        error_code = "missing_llm_field";
        message = "LLM response is missing a required planning field.";
        return false;
    }

    proposal.trace_id = trace_id;
    proposal.target_object_id = target_object_id;
    proposal.operation_type = operation_type;
    proposal.tool_id = tool_id;
    proposal.stepover = stepover;
    proposal.stepdown = stepdown;
    proposal.tolerance = tolerance;
    return true;
}

} // namespace camclaw
