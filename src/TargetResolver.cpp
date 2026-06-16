#include "camclaw/TargetResolver.h"

#include <sstream>

namespace camclaw {

TargetResolver::TargetResolver(Repository& repository)
    : repository_(repository)
{
}

TargetResolutionResult TargetResolver::resolveOperation(const std::map<std::string, std::string>& query) const
{
    TargetResolutionResult result;
    const std::map<std::string, std::string>::const_iterator target_id = query.find("target_object_id");
    if (target_id != query.end() && !target_id->second.empty()) {
        const CamObject target = repository_.get(target_id->second);
        if (target.object_type == ObjectType::Operation) {
            result.status = TargetResolutionStatus::Resolved;
            result.target_object_id = target.object_id;
            return result;
        }
        result.status = TargetResolutionStatus::NotFound;
        result.error_code = "invalid_target_type";
        result.message = "Resolved target is not an operation.";
        return result;
    }

    const std::map<std::string, std::string>::const_iterator operation_type = query.find("operation_type");
    if (operation_type == query.end() || operation_type->second.empty()) {
        result.status = TargetResolutionStatus::NotFound;
        result.error_code = "missing_target_query";
        result.message = "Operation target resolution requires target_object_id or operation_type.";
        return result;
    }

    const std::vector<CamObject> operations = repository_.objectsByType(ObjectType::Operation);
    for (std::size_t index = 0u; index < operations.size(); ++index) {
        const std::map<std::string, std::string>::const_iterator type = operations[index].attributes.find("operation_type");
        if (type != operations[index].attributes.end()
            && operationTypeMatchesFilter(type->second, operation_type->second)) {
            result.candidates.push_back(ClarificationCandidate(
                operations[index].object_id,
                operations[index].display_name,
                candidateDescription(operations[index])));
        }
    }

    if (result.candidates.empty()) {
        result.status = TargetResolutionStatus::NotFound;
        result.error_code = "target_not_found";
        result.message = "No operation matches the requested target query.";
        return result;
    }

    if (result.candidates.size() == 1u) {
        result.status = TargetResolutionStatus::Resolved;
        result.target_object_id = result.candidates[0].id;
        return result;
    }

    result.status = TargetResolutionStatus::NeedsClarification;
    result.error_code = "ambiguous_target";
    result.message = "Multiple operations match the requested target query.";
    return result;
}

bool TargetResolver::operationTypeMatchesFilter(const std::string& operation_type, const std::string& filter) const
{
    if (filter == "pocket") {
        return operation_type == "pocket" || operation_type == "roughing" || operation_type == "pocket_roughing";
    }
    return operation_type == filter;
}

std::string TargetResolver::candidateDescription(const CamObject& operation) const
{
    std::ostringstream stream;
    const std::map<std::string, std::string>::const_iterator tool_id = operation.attributes.find("tool_id");
    const std::map<std::string, std::string>::const_iterator stepover = operation.attributes.find("stepover");
    if (tool_id != operation.attributes.end()) {
        stream << "tool_id=" << tool_id->second;
    }
    if (stepover != operation.attributes.end()) {
        if (tool_id != operation.attributes.end()) {
            stream << ", ";
        }
        stream << "stepover=" << stepover->second;
    }
    return stream.str();
}

} // namespace camclaw
