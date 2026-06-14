#include "camclaw/OpenAiCompatibleLlmClient.h"

#include <cstdlib>
#include <iostream>
#include <string>

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

class FakeTransport : public camclaw::HttpTransport {
public:
    camclaw::HttpResponse send(const camclaw::HttpRequest& request)
    {
        last_request = request;

        camclaw::HttpResponse response;
        response.status_code = 200;
        response.body =
            "{"
            "\"choices\":[{"
            "\"message\":{"
            "\"content\":\"{\\\"operation_type\\\":\\\"roughing\\\",\\\"tool_id\\\":\\\"tool_010\\\",\\\"stepover\\\":\\\"2.0\\\",\\\"stepdown\\\":\\\"1.0\\\",\\\"tolerance\\\":\\\"0.02\\\"}\""
            "}"
            "}]"
            "}";
        return response;
    }

    camclaw::HttpRequest last_request;
};

static int client_sends_openai_compatible_chat_request()
{
    camclaw::LlmEndpointConfig config;
    config.base_url = "https://llm.example";
    config.api_key = "local-secret";
    config.model = "gpt-5.5";

    FakeTransport transport;
    camclaw::OpenAiCompatibleLlmClient client(config, transport);

    camclaw::LlmPlanRequest request;
    request.trace_id = "trace_http_001";
    request.target_object_id = "feature_001";
    request.user_request = "给当前型腔做粗加工";
    request.response_contract = camclaw::LlmPlanningContract::roughingPlanJsonContract();

    const std::string plan_json = client.createPlanJson(request);

    REQUIRE_EQ(std::string("POST"), transport.last_request.method);
    REQUIRE_EQ(std::string("https://llm.example/v1/chat/completions"), transport.last_request.url);
    REQUIRE_EQ(std::string("application/json"), transport.last_request.headers["Content-Type"]);
    REQUIRE_EQ(std::string("Bearer local-secret"), transport.last_request.headers["Authorization"]);
    REQUIRE_TRUE(transport.last_request.body.find("\"model\":\"gpt-5.5\"") != std::string::npos);
    REQUIRE_TRUE(transport.last_request.body.find("operation_type") != std::string::npos);
    REQUIRE_TRUE(transport.last_request.body.find("feature_001") != std::string::npos);
    REQUIRE_TRUE(plan_json.find("\"operation_type\":\"roughing\"") != std::string::npos);

    return EXIT_SUCCESS;
}

int main()
{
    return client_sends_openai_compatible_chat_request();
}
