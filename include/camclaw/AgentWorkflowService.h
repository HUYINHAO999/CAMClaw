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
    std::string parent_object_id;
    std::map<std::string, std::string> attributes;
};

class Repository {
public:
    bool save(const CamObject& object);
    bool exists(const std::string& object_id) const;
    CamObject get(const std::string& object_id) const;
    bool update(const CamObject& object);
    bool remove(const std::string& object_id);
    std::vector<CamObject> objectsByType(ObjectType object_type) const;

private:
    std::map<std::string, CamObject> objects_;
};

struct OperationDefinition {
    OperationDefinition()
    {
    }

    OperationDefinition(
        const std::string& type,
        const std::string& name,
        const std::map<std::string, std::string>& defaults)
        : operation_type(type),
          display_name(name),
          default_parameters(defaults)
    {
    }

    std::string operation_type;
    std::string display_name;
    std::map<std::string, std::string> default_parameters;
};

class OperationCatalog {
public:
    static OperationCatalog defaults();

    void registerDefinition(const OperationDefinition& definition);
    bool find(const std::string& operation_type, OperationDefinition& definition) const;
    std::vector<OperationDefinition> definitions() const;

private:
    std::map<std::string, OperationDefinition> definitions_;
};

struct CreateOperationRequest {
    std::string operation_type;
    std::string target_object_id;
    std::map<std::string, std::string> parameters;
};

class OperationFactory {
public:
    explicit OperationFactory(const OperationCatalog& catalog);

    bool create(
        Repository& repository,
        const CreateOperationRequest& request,
        CamObject& operation,
        std::string& error_code,
        std::string& message) const;

private:
    std::string baseObjectId(const CreateOperationRequest& request) const;
    std::string uniqueObjectId(Repository& repository, const std::string& base_id) const;

    const OperationCatalog& catalog_;
};

class OperationService {
public:
    OperationService(Repository& repository, const OperationFactory& factory);

    bool createOperation(
        const CreateOperationRequest& request,
        CamObject& operation,
        std::string& error_code,
        std::string& message) const;
    bool updateOperationParameter(
        const std::string& operation_object_id,
        const std::string& parameter_name,
        const std::string& parameter_value,
        std::string& error_code,
        std::string& message) const;

private:
    Repository& repository_;
    const OperationFactory& factory_;
};

class ToolPathService {
public:
    explicit ToolPathService(Repository& repository);

    bool generateToolPath(
        const std::string& operation_object_id,
        CamObject& toolpath,
        std::string& error_code,
        std::string& message) const;

    bool deleteToolPathForOperation(const std::string& operation_object_id);
    bool setToolPathVisibility(
        const std::string& toolpath_object_id,
        bool visible,
        std::string& error_code,
        std::string& message);
    bool toggleToolPathVisibility(
        const std::string& toolpath_object_id,
        bool& visible,
        std::string& error_code,
        std::string& message);
    std::vector<std::string> setAllToolPathVisibility(bool visible);

private:
    Repository& repository_;
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
