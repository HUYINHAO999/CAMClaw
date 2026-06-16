#include "camclaw/HumanInLoopService.h"

#include <sstream>

namespace camclaw {

ClarificationRequest HumanInLoopService::createRequest(
    const std::string& trace_id,
    std::size_t blocked_action_index,
    const std::string& reason,
    const std::string& subject_kind,
    const std::string& question,
    const std::map<std::string, std::string>& query,
    const std::vector<ClarificationCandidate>& candidates,
    const ResponseSchema& response_schema)
{
    ClarificationRequest request;
    request.clarification_id = nextClarificationId(trace_id);
    request.trace_id = trace_id;
    request.reason = reason;
    request.subject_kind = subject_kind;
    request.question = question;
    request.query = query;
    request.candidates = candidates;
    request.response_schema = response_schema;
    request.blocked_action_index = blocked_action_index;

    pending_.insert(std::make_pair(request.clarification_id, PendingClarification(request)));
    return request;
}

ClarificationResumeResult HumanInLoopService::submitResponse(const ClarificationResponse& response)
{
    ClarificationResumeResult result;
    const std::map<std::string, PendingClarification>::iterator found = pending_.find(response.clarification_id);
    if (found == pending_.end()) {
        result.error_code = "clarification_not_found";
        result.message = "Clarification session was not found.";
        return result;
    }

    const std::string selected_id = resolveSelectedCandidate(found->second, response);
    if (selected_id.empty()) {
        result.error_code = "invalid_clarification_response";
        result.message = "Clarification response did not identify a candidate.";
        return result;
    }

    pending_.erase(found);
    result.ok = true;
    result.selected_ids.push_back(selected_id);
    return result;
}

std::vector<ClarificationRequest> HumanInLoopService::pendingRequests() const
{
    std::vector<ClarificationRequest> result;
    for (std::map<std::string, PendingClarification>::const_iterator it = pending_.begin(); it != pending_.end(); ++it) {
        result.push_back(it->second.request);
    }
    return result;
}

bool HumanInLoopService::cancel(const std::string& clarification_id)
{
    return pending_.erase(clarification_id) > 0u;
}

std::string HumanInLoopService::nextClarificationId(const std::string& trace_id) const
{
    std::ostringstream stream;
    stream << "clarify_" << trace_id << "_" << (pending_.size() + 1u);
    return stream.str();
}

std::string HumanInLoopService::resolveSelectedCandidate(
    const PendingClarification& pending,
    const ClarificationResponse& response) const
{
    if (!response.selected_ids.empty()) {
        return response.selected_ids[0];
    }

    if (pending.request.response_schema.allow_free_text && matchesLastCandidateText(response.free_text)) {
        if (!pending.request.candidates.empty()) {
            return pending.request.candidates.back().id;
        }
    }

    for (std::size_t index = 0; index < pending.request.candidates.size(); ++index) {
        if (response.free_text == pending.request.candidates[index].id
            || response.free_text == pending.request.candidates[index].label) {
            return pending.request.candidates[index].id;
        }
    }

    return std::string();
}

bool HumanInLoopService::matchesLastCandidateText(const std::string& text) const
{
    return text == "最后一个" || text == "最后一个型腔铣工序" || text == "last" || text == "last one";
}

} // namespace camclaw
