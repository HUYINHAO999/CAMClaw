#ifndef CAMCLAW_TARGET_RESOLVER_H
#define CAMCLAW_TARGET_RESOLVER_H

#include "camclaw/AgentWorkflowService.h"
#include "camclaw/HumanInLoopService.h"

#include <map>
#include <string>
#include <vector>

namespace camclaw {

enum class TargetResolutionStatus {
    Resolved,
    NeedsClarification,
    NotFound
};

struct TargetResolutionResult {
    TargetResolutionResult()
        : status(TargetResolutionStatus::NotFound)
    {
    }

    TargetResolutionStatus status;
    std::string target_object_id;
    std::string error_code;
    std::string message;
    std::vector<ClarificationCandidate> candidates;
};

class TargetResolver {
public:
    explicit TargetResolver(Repository& repository);

    TargetResolutionResult resolveOperation(const std::map<std::string, std::string>& query) const;

private:
    bool operationTypeMatchesFilter(const std::string& operation_type, const std::string& filter) const;
    std::string candidateDescription(const CamObject& operation) const;

    Repository& repository_;
};

} // namespace camclaw

#endif // CAMCLAW_TARGET_RESOLVER_H
