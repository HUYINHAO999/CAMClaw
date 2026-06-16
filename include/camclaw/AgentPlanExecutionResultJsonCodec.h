#ifndef CAMCLAW_AGENT_PLAN_EXECUTION_RESULT_JSON_CODEC_H
#define CAMCLAW_AGENT_PLAN_EXECUTION_RESULT_JSON_CODEC_H

#include "camclaw/AgentPlanExecutionResult.h"

#include <string>

namespace camclaw {

class AgentPlanExecutionResultJsonCodec {
public:
    std::string encode(const AgentPlanExecutionResult& result) const;
};

} // namespace camclaw

#endif // CAMCLAW_AGENT_PLAN_EXECUTION_RESULT_JSON_CODEC_H
