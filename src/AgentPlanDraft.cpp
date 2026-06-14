#include "camclaw/AgentPlanDraft.h"

namespace camclaw {

SkillStepDraft::SkillStepDraft(const std::string& skill_id, const std::map<std::string, std::string>& inputs)
    : skill_id_(skill_id),
      inputs_(inputs)
{
}

const std::string& SkillStepDraft::skillId() const
{
    return skill_id_;
}

bool SkillStepDraft::hasInput(const std::string& name) const
{
    return inputs_.find(name) != inputs_.end();
}

std::string SkillStepDraft::inputValue(const std::string& name) const
{
    const std::map<std::string, std::string>::const_iterator found = inputs_.find(name);
    if (found == inputs_.end()) {
        return std::string();
    }

    return found->second;
}

const std::map<std::string, std::string>& SkillStepDraft::inputs() const
{
    return inputs_;
}

void SkillStepDraft::setInputValue(const std::string& name, const std::string& value)
{
    inputs_[name] = value;
}

AgentPlanDraft::AgentPlanDraft(const std::string& trace_id)
    : trace_id_(trace_id),
      status_(DraftStatus::PendingReview)
{
    trace_events_.push_back("draft_created");
}

const std::string& AgentPlanDraft::traceId() const
{
    return trace_id_;
}

DraftStatus AgentPlanDraft::status() const
{
    return status_;
}

std::size_t AgentPlanDraft::stepCount() const
{
    return steps_.size();
}

const SkillStepDraft& AgentPlanDraft::stepAt(std::size_t index) const
{
    return steps_.at(index);
}

const std::vector<std::string>& AgentPlanDraft::traceEvents() const
{
    return trace_events_;
}

const std::string& AgentPlanDraft::rejectionReason() const
{
    return rejection_reason_;
}

std::size_t AgentPlanDraft::planningContextCount() const
{
    return planning_context_.size();
}

PlanningContextItem AgentPlanDraft::planningContextAt(std::size_t index) const
{
    return planning_context_.at(index);
}

void AgentPlanDraft::addSkillStep(const SkillStepDraft& step)
{
    if (status_ != DraftStatus::PendingReview) {
        return;
    }

    steps_.push_back(step);
}

void AgentPlanDraft::addPlanningContext(const PlanningContextItem& item)
{
    if (status_ != DraftStatus::PendingReview) {
        return;
    }

    planning_context_.push_back(item);
    trace_events_.push_back("planning_context_attached");
}

DraftEditResult AgentPlanDraft::editStepInput(std::size_t step_index, const std::string& input_name, const std::string& value)
{
    DraftEditResult result;

    if (status_ != DraftStatus::PendingReview) {
        result.status = DraftEditStatus::DraftClosed;
        result.message = "Draft is no longer editable.";
        return result;
    }

    if (step_index >= steps_.size()) {
        result.status = DraftEditStatus::StepNotFound;
        result.message = "Draft step does not exist.";
        return result;
    }

    if (!steps_[step_index].hasInput(input_name)) {
        result.status = DraftEditStatus::InputNotFound;
        result.message = "Draft input does not exist.";
        return result;
    }

    steps_[step_index].setInputValue(input_name, value);
    trace_events_.push_back("draft_input_edited");
    return result;
}

void AgentPlanDraft::confirm()
{
    if (status_ != DraftStatus::PendingReview) {
        return;
    }

    status_ = DraftStatus::Confirmed;
    trace_events_.push_back("draft_confirmed_final");
}

void AgentPlanDraft::reject(const std::string& reason)
{
    if (status_ != DraftStatus::PendingReview) {
        return;
    }

    status_ = DraftStatus::Rejected;
    rejection_reason_ = reason;
    trace_events_.push_back("draft_rejected");
}

} // namespace camclaw
