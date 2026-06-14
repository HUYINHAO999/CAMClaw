#include "camclaw/AgentPlanExecutionResultJsonCodec.h"

namespace camclaw {

namespace {

std::string jsonEscape(const std::string& value)
{
    std::string escaped;
    for (std::size_t index = 0u; index < value.size(); ++index) {
        const char ch = value[index];
        if (ch == '\\' || ch == '"') {
            escaped.push_back('\\');
            escaped.push_back(ch);
        } else if (ch == '\n') {
            escaped += "\\n";
        } else if (ch == '\r') {
            escaped += "\\r";
        } else if (ch == '\t') {
            escaped += "\\t";
        } else {
            escaped.push_back(ch);
        }
    }
    return escaped;
}

std::string encodeStringArray(const std::vector<std::string>& values)
{
    std::string json = "[";
    for (std::size_t index = 0u; index < values.size(); ++index) {
        if (index != 0u) {
            json += ",";
        }
        json += "\"" + jsonEscape(values[index]) + "\"";
    }
    json += "]";
    return json;
}

} // namespace

std::string AgentPlanExecutionResultJsonCodec::encode(const AgentPlanExecutionResult& result) const
{
    std::string json = "{";
    json += "\"ok\":";
    json += result.ok ? "true" : "false";
    json += ",\"error_code\":\"" + jsonEscape(result.error_code) + "\"";
    json += ",\"message\":\"" + jsonEscape(result.message) + "\"";
    json += ",\"primary_object_id\":\"" + jsonEscape(result.primary_object_id) + "\"";
    json += ",\"object_ids\":" + encodeStringArray(result.object_ids);
    json += ",\"trace_events\":" + encodeStringArray(result.trace_events);
    json += "}";
    return json;
}

} // namespace camclaw
