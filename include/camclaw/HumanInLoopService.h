#ifndef CAMCLAW_HUMAN_IN_LOOP_SERVICE_H
#define CAMCLAW_HUMAN_IN_LOOP_SERVICE_H

#include <map>
#include <string>
#include <vector>

namespace camclaw {

struct ClarificationCandidate {
    ClarificationCandidate()
    {
    }

    ClarificationCandidate(
        const std::string& candidate_id,
        const std::string& candidate_label,
        const std::string& candidate_description)
        : id(candidate_id),
          label(candidate_label),
          description(candidate_description)
    {
    }

    std::string id;
    std::string label;
    std::string description;
};

struct ResponseSchema {
    ResponseSchema()
        : allow_free_text(false)
    {
    }

    std::string type;
    std::string target_field;
    bool allow_free_text;
};

struct ClarificationRequest {
    ClarificationRequest()
        : blocked_action_index(0u)
    {
    }

    std::string clarification_id;
    std::string trace_id;
    std::string reason;
    std::string subject_kind;
    std::string question;
    std::map<std::string, std::string> query;
    std::vector<ClarificationCandidate> candidates;
    ResponseSchema response_schema;
    std::size_t blocked_action_index;
};

struct ClarificationResponse {
    std::string clarification_id;
    std::vector<std::string> selected_ids;
    std::string free_text;
    std::map<std::string, std::string> form_values;
};

struct ClarificationResumeResult {
    ClarificationResumeResult()
        : ok(false)
    {
    }

    bool ok;
    std::string error_code;
    std::string message;
    std::vector<std::string> selected_ids;
};

class HumanInLoopService {
public:
    ClarificationRequest createRequest(
        const std::string& trace_id,
        std::size_t blocked_action_index,
        const std::string& reason,
        const std::string& subject_kind,
        const std::string& question,
        const std::map<std::string, std::string>& query,
        const std::vector<ClarificationCandidate>& candidates,
        const ResponseSchema& response_schema);

    ClarificationResumeResult submitResponse(const ClarificationResponse& response);
    std::vector<ClarificationRequest> pendingRequests() const;
    bool cancel(const std::string& clarification_id);

private:
    struct PendingClarification {
        ClarificationRequest request;

        explicit PendingClarification(const ClarificationRequest& request_value)
            : request(request_value)
        {
        }
    };

    std::string nextClarificationId(const std::string& trace_id) const;
    std::string resolveSelectedCandidate(const PendingClarification& pending, const ClarificationResponse& response) const;
    bool matchesLastCandidateText(const std::string& text) const;

    std::map<std::string, PendingClarification> pending_;
};

} // namespace camclaw

#endif // CAMCLAW_HUMAN_IN_LOOP_SERVICE_H
