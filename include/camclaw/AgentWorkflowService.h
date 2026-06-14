#ifndef CAMCLAW_AGENT_WORKFLOW_SERVICE_H
#define CAMCLAW_AGENT_WORKFLOW_SERVICE_H

#include <map>
#include <string>
#include <vector>

namespace camclaw {

enum class ObjectType {
    Unknown,
    Feature,
    Operation,
    Toolpath
};

enum class WorkflowStatus {
    Completed,
    NeedsTargetConfirmation,
    NeedsValidTarget,
    Failed
};

struct CamObject {
    CamObject()
        : object_type(ObjectType::Unknown)
    {
    }

    CamObject(const std::string& id, ObjectType type, const std::string& name)
        : object_id(id),
          object_type(type),
          display_name(name)
    {
    }

    std::string object_id;
    ObjectType object_type;
    std::string display_name;
};

class Repository {
public:
    void save(const CamObject& object);
    bool exists(const std::string& object_id) const;
    CamObject get(const std::string& object_id) const;

private:
    std::map<std::string, CamObject> objects_;
};

struct SelectionCandidate {
    SelectionCandidate()
        : has_object(false),
          object_type(ObjectType::Unknown)
    {
    }

    bool has_object;
    std::string object_id;
    ObjectType object_type;
    std::string display_name;
};

struct RoughingWorkflowRequest {
    std::string trace_id;
    std::string target_object_id;
    std::string operation_type;
    std::string tool_id;
    std::string stepover;
    std::string stepdown;
    std::string tolerance;
    SelectionCandidate active_selection;
};

struct TargetConfirmation {
    TargetConfirmation()
        : target_object_type(ObjectType::Unknown)
    {
    }

    std::string target_object_id;
    ObjectType target_object_type;
    std::string target_display_name;
    std::string message;
};

struct WorkflowResult {
    WorkflowResult()
        : status(WorkflowStatus::Failed)
    {
    }

    WorkflowStatus status;
    TargetConfirmation confirmation;
    std::string primary_object_id;
    std::vector<std::string> created_object_ids;
    std::vector<std::string> trace_events;
    std::string error_code;
    std::string message;
};

class AgentWorkflowService {
public:
    explicit AgentWorkflowService(Repository& repository);

    WorkflowResult submitRoughingAndToolpath(const RoughingWorkflowRequest& request);

private:
    Repository& repository_;
};

} // namespace camclaw

#endif // CAMCLAW_AGENT_WORKFLOW_SERVICE_H
