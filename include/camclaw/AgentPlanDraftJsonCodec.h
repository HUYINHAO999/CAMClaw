#ifndef CAMCLAW_AGENT_PLAN_DRAFT_JSON_CODEC_H
#define CAMCLAW_AGENT_PLAN_DRAFT_JSON_CODEC_H

#include "camclaw/AgentPlanDraft.h"

#include <string>

namespace camclaw {

struct AgentPlanDraftDecodeResult {
    AgentPlanDraftDecodeResult();

    bool ok;
    AgentPlanDraft draft;
    std::string error_code;
    std::string message;
};

class AgentPlanDraftJsonCodec {
public:
    AgentPlanDraftDecodeResult decodeBackendDraft(const std::string& json) const;
};

} // namespace camclaw

#endif // CAMCLAW_AGENT_PLAN_DRAFT_JSON_CODEC_H
