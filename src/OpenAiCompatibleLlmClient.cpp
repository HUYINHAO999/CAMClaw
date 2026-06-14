#include "camclaw/OpenAiCompatibleLlmClient.h"

namespace camclaw {

namespace {

std::string jsonEscape(const std::string& value)
{
    std::string escaped;
    for (std::size_t index = 0; index < value.size(); ++index) {
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

bool findJsonStringValue(const std::string& json, const std::string& key, std::string& value)
{
    const std::string needle = "\"" + key + "\"";
    const std::size_t key_pos = json.find(needle);
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
            const char escaped = json[quote_pos++];
            if (escaped == 'n') {
                value.push_back('\n');
            } else if (escaped == 'r') {
                value.push_back('\r');
            } else if (escaped == 't') {
                value.push_back('\t');
            } else {
                value.push_back(escaped);
            }
            continue;
        }

        value.push_back(ch);
    }

    return false;
}

} // namespace

OpenAiCompatibleLlmClient::OpenAiCompatibleLlmClient(const LlmEndpointConfig& config, HttpTransport& transport)
    : config_(config),
      transport_(transport)
{
}

std::string OpenAiCompatibleLlmClient::createPlanJson(const LlmPlanRequest& request)
{
    HttpRequest http_request;
    http_request.method = "POST";
    http_request.url = buildUrl();
    http_request.headers["Content-Type"] = "application/json";
    http_request.headers["Authorization"] = "Bearer " + config_.api_key;
    http_request.body = buildRequestBody(request);

    const HttpResponse response = transport_.send(http_request);
    if (response.status_code < 200 || response.status_code >= 300) {
        return std::string();
    }

    return extractMessageContent(response.body);
}

std::string OpenAiCompatibleLlmClient::buildUrl() const
{
    if (config_.base_url.empty()) {
        return "/v1/chat/completions";
    }

    if (config_.base_url[config_.base_url.size() - 1u] == '/') {
        return config_.base_url + "v1/chat/completions";
    }

    return config_.base_url + "/v1/chat/completions";
}

std::string OpenAiCompatibleLlmClient::buildRequestBody(const LlmPlanRequest& request) const
{
    const std::string system_content =
        "You produce CAM AgentPlanDraft inputs. "
        + request.response_contract;

    const std::string user_content =
        "trace_id=" + request.trace_id
        + "\ntarget_object_id=" + request.target_object_id
        + "\nuser_request=" + request.user_request;

    return std::string()
        + "{"
        + "\"model\":\"" + jsonEscape(config_.model) + "\","
        + "\"messages\":["
        + "{\"role\":\"system\",\"content\":\"" + jsonEscape(system_content) + "\"},"
        + "{\"role\":\"user\",\"content\":\"" + jsonEscape(user_content) + "\"}"
        + "]"
        + "}";
}

std::string OpenAiCompatibleLlmClient::extractMessageContent(const std::string& response_body) const
{
    std::string content;
    if (!findJsonStringValue(response_body, "content", content)) {
        return std::string();
    }

    return content;
}

} // namespace camclaw
