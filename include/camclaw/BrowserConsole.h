#ifndef CAMCLAW_BROWSER_CONSOLE_H
#define CAMCLAW_BROWSER_CONSOLE_H

#include "camclaw/AgentWorkflowService.h"

#include <map>
#include <string>
#include <vector>

namespace camclaw {

struct BrowserConsoleResult {
    BrowserConsoleResult()
        : ok(false)
    {
    }

    bool ok;
    std::string error_code;
    std::string message;
    std::string primary_object_id;
    std::vector<std::string> object_ids;
    std::vector<std::string> trace_events;
};

struct BrowserOperationCreateItem {
    BrowserOperationCreateItem()
        : count(1)
    {
    }

    std::string operation_type;
    int count;
};

struct BrowserParameterUpdate {
    std::string parameter;
    std::string value;
    std::string expression;
};

class BrowserConsoleUi {
public:
    virtual ~BrowserConsoleUi() {}

    virtual void refreshBrowserTree() = 0;
    virtual void selectObject(const std::string& object_id) = 0;
    virtual void showOperationPreview(const std::string& operation_id) = 0;
    virtual void showToolpathPreview(const std::string& toolpath_id) = 0;
    virtual void syncVisibleToolpaths() = 0;
    virtual void openOperationEditor(const std::string& operation_id) = 0;
};

class FeatureRecognitionService {
public:
    std::string recognizeOperationType(const CamObject& feature) const;
};

class ToolSelectionPolicy {
public:
    std::string resolveRelativeToolId(const CamObject& operation, const std::string& expression) const;
};

class ParameterExpressionResolver {
public:
    explicit ParameterExpressionResolver(const ToolSelectionPolicy& tool_selection_policy);

    std::string resolve(const CamObject& operation, const BrowserParameterUpdate& update) const;

private:
    std::string resolveRelativeNumber(
        const CamObject& operation,
        const std::string& parameter_name,
        const std::string& expression) const;

    const ToolSelectionPolicy& tool_selection_policy_;
};

class BrowserConsole {
public:
    explicit BrowserConsole(Repository& repository, BrowserConsoleUi* ui = 0);

    Repository& repository();
    const Repository& repository() const;
    void setUi(BrowserConsoleUi* ui);

    BrowserConsoleResult machineFeature(
        const std::vector<std::string>& feature_ids,
        bool auto_generate_toolpath);
    BrowserConsoleResult createOperations(
        const std::vector<BrowserOperationCreateItem>& items,
        const std::string& target_feature_id,
        bool auto_generate_toolpath);
    BrowserConsoleResult updateOperations(
        const std::vector<std::string>& operation_ids,
        const std::vector<BrowserParameterUpdate>& updates,
        bool open_editor);
    BrowserConsoleResult generateToolpaths(const std::vector<std::string>& operation_ids);
    BrowserConsoleResult regenerateToolpaths(const std::vector<std::string>& operation_ids);
    BrowserConsoleResult deleteToolpathForOperation(const std::string& operation_id);
    BrowserConsoleResult setToolpathVisibility(
        const std::vector<std::string>& toolpath_ids,
        const std::string& visibility);

private:
    void refreshAndSelect(const BrowserConsoleResult& result);
    void appendObject(BrowserConsoleResult& result, const std::string& object_id) const;
    OperationService makeOperationService(OperationFactory& factory) const;

    Repository& repository_;
    BrowserConsoleUi* ui_;
    FeatureRecognitionService feature_recognition_service_;
    ToolSelectionPolicy tool_selection_policy_;
    ParameterExpressionResolver parameter_expression_resolver_;
};

} // namespace camclaw

#endif // CAMCLAW_BROWSER_CONSOLE_H
