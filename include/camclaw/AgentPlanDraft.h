#ifndef CAMCLAW_AGENT_PLAN_DRAFT_H
#define CAMCLAW_AGENT_PLAN_DRAFT_H

#include <map>
#include <string>
#include <vector>

namespace camclaw {

enum class DraftStatus {
    PendingReview,
    Confirmed,
    Rejected
};

enum class DraftEditStatus {
    Edited,
    StepNotFound,
    InputNotFound,
    DraftClosed
};

struct DraftEditResult {
    DraftEditResult()
        : status(DraftEditStatus::Edited)
    {
    }

    DraftEditStatus status;
    std::string message;
};

struct PlanningContextItem {
    PlanningContextItem()
    {
    }

    PlanningContextItem(const std::string& source_name, const std::string& text_value)
        : source(source_name),
          text(text_value)
    {
    }

    std::string source;
    std::string text;
};

class SkillStepDraft {
public:
    SkillStepDraft(const std::string& skill_id, const std::map<std::string, std::string>& inputs);

    const std::string& skillId() const;
    bool hasInput(const std::string& name) const;
    std::string inputValue(const std::string& name) const;
    const std::map<std::string, std::string>& inputs() const;

private:
    friend class AgentPlanDraft;

    void setInputValue(const std::string& name, const std::string& value);

    std::string skill_id_;
    std::map<std::string, std::string> inputs_;
};

class AgentPlanDraft {
public:
    explicit AgentPlanDraft(const std::string& trace_id);

    const std::string& traceId() const;
    DraftStatus status() const;
    std::size_t stepCount() const;
    const SkillStepDraft& stepAt(std::size_t index) const;
    const std::vector<std::string>& traceEvents() const;
    const std::string& rejectionReason() const;
    std::size_t planningContextCount() const;
    PlanningContextItem planningContextAt(std::size_t index) const;

    void addSkillStep(const SkillStepDraft& step);
    void addPlanningContext(const PlanningContextItem& item);
    DraftEditResult editStepInput(std::size_t step_index, const std::string& input_name, const std::string& value);
    DraftEditResult setStepInput(std::size_t step_index, const std::string& input_name, const std::string& value);
    void confirm();
    void reject(const std::string& reason);

private:
    std::string trace_id_;
    DraftStatus status_;
    std::vector<SkillStepDraft> steps_;
    std::vector<std::string> trace_events_;
    std::vector<PlanningContextItem> planning_context_;
    std::string rejection_reason_;
};

} // namespace camclaw

#endif // CAMCLAW_AGENT_PLAN_DRAFT_H
