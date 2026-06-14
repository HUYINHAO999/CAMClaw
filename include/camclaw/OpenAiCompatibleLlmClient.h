#ifndef CAMCLAW_OPENAI_COMPATIBLE_LLM_CLIENT_H
#define CAMCLAW_OPENAI_COMPATIBLE_LLM_CLIENT_H

#include "camclaw/AgentPlanner.h"

#include <map>
#include <string>

namespace camclaw {

struct HttpRequest {
    std::string method;
    std::string url;
    std::map<std::string, std::string> headers;
    std::string body;
};

struct HttpResponse {
    HttpResponse()
        : status_code(0)
    {
    }

    int status_code;
    std::string body;
};

class HttpTransport {
public:
    virtual ~HttpTransport()
    {
    }

    virtual HttpResponse send(const HttpRequest& request) = 0;
};

class OpenAiCompatibleLlmClient : public LlmClient {
public:
    OpenAiCompatibleLlmClient(const LlmEndpointConfig& config, HttpTransport& transport);

    std::string createPlanJson(const LlmPlanRequest& request);

private:
    std::string buildUrl() const;
    std::string buildRequestBody(const LlmPlanRequest& request) const;
    std::string extractMessageContent(const std::string& response_body) const;

    LlmEndpointConfig config_;
    HttpTransport& transport_;
};

} // namespace camclaw

#endif // CAMCLAW_OPENAI_COMPATIBLE_LLM_CLIENT_H
