from __future__ import annotations

import json
import re
from dataclasses import dataclass
from typing import Dict, List, Protocol

from .contracts import REQUIRED_ROUGHING_FIELDS, cam_action_plan_json_contract, roughing_plan_json_contract
from .feature_recognition import FeatureRecognizer
from .tool_library import ToolLibrary


class LlmPlanningClient(Protocol):
    def create_plan_json(
        self,
        *,
        trace_id: str,
        user_request: str,
        target_object_id: str,
        rejection_reason: str,
        response_contract: str,
    ) -> str:
        ...


@dataclass(frozen=True)
class PlannerInput:
    trace_id: str
    user_request: str
    target_object_id: str
    rejection_reason: str = ""
    target_display_name: str = ""


class PlannerError(ValueError):
    def __init__(self, error_code: str, message: str):
        super().__init__(message)
        self.error_code = error_code
        self.message = message


class AgentPlanner:
    def __init__(
        self,
        llm_client: LlmPlanningClient,
        tool_library: ToolLibrary | None = None,
        feature_recognizer: FeatureRecognizer | None = None,
    ):
        self._llm_client = llm_client
        self._tool_library = tool_library or ToolLibrary.from_file()
        self._feature_recognizer = feature_recognizer or FeatureRecognizer()

    def create_draft(self, planner_input: PlannerInput) -> Dict[str, object]:
        plan_json = self._llm_client.create_plan_json(
            trace_id=planner_input.trace_id,
            user_request=planner_input.user_request,
            target_object_id=planner_input.target_object_id,
            rejection_reason=planner_input.rejection_reason,
            response_contract=cam_action_plan_json_contract(self._tool_library.to_planning_context()),
        )
        parsed = self._parse_json_object(plan_json)
        if "command_id" not in parsed:
            if "actions" in parsed:
                return self._create_action_sequence_draft(planner_input, parsed)
            return self._create_legacy_roughing_draft(planner_input, parsed)

        step = self._build_step_from_action(parsed, planner_input)
        return {
            "schema_version": "1",
            "trace_id": planner_input.trace_id,
            "status": "pending_review",
            "steps": [step],
            "trace_events": ["draft_created"],
        }

    def _create_action_sequence_draft(self, planner_input: PlannerInput, parsed: Dict[str, object]) -> Dict[str, object]:
        actions = parsed.get("actions")
        if not isinstance(actions, list) or not actions:
            raise PlannerError("invalid_llm_response", "LLM response actions must be a non-empty array.")

        steps = []
        normalized_actions = self._normalize_visibility_action_targets(actions, planner_input)
        for action in normalized_actions:
            if not isinstance(action, dict):
                raise PlannerError("invalid_llm_response", "Each action must be a JSON object.")
            steps.append(self._build_step_from_action(action, planner_input))

        return {
            "schema_version": "1",
            "trace_id": planner_input.trace_id,
            "status": "pending_review",
            "steps": steps,
            "trace_events": ["draft_created"],
        }

    def _normalize_visibility_action_targets(
        self,
        actions: List[object],
        planner_input: PlannerInput,
    ) -> List[object]:
        operation_types = self._operation_type_filters_for_visibility(planner_input.user_request)
        if not operation_types:
            return actions

        visibility_actions = [
            action for action in actions
            if isinstance(action, dict)
            and action.get("command_id") == "browser.setToolpathVisibility"
            and isinstance(action.get("inputs", {}), dict)
        ]
        missing_target_actions = [
            action for action in visibility_actions
            if not str(action.get("inputs", {}).get("operation_type", ""))
            and str(action.get("inputs", {}).get("scope", "")) == "operation_type"
        ]
        if not missing_target_actions:
            return actions

        if len(visibility_actions) == 1 and len(missing_target_actions) == 1 and len(operation_types) > 1:
            expanded_actions: List[object] = []
            for action in actions:
                if action is visibility_actions[0]:
                    for operation_type in operation_types:
                        expanded_actions.append(self._copy_action_with_operation_type(action, operation_type))
                else:
                    expanded_actions.append(action)
            return expanded_actions

        assigned_index = 0
        normalized_actions = []
        for action in actions:
            if action in missing_target_actions and assigned_index < len(operation_types):
                normalized_actions.append(self._copy_action_with_operation_type(action, operation_types[assigned_index]))
                assigned_index += 1
            else:
                normalized_actions.append(action)
        return normalized_actions

    @staticmethod
    def _copy_action_with_operation_type(action: Dict[str, object], operation_type: str) -> Dict[str, object]:
        copied = dict(action)
        inputs = dict(action.get("inputs", {}))
        inputs["scope"] = "operation_type"
        inputs["operation_type"] = operation_type
        inputs["toolpath_ids"] = ""
        copied["inputs"] = inputs
        return copied

    def _build_step_from_action(self, action: Dict[str, object], planner_input: PlannerInput) -> Dict[str, object]:
        command_id = self._required_string(action, "command_id")
        schema_id = self._required_string(action, "schema_id")
        inputs = action.get("inputs", {})
        if not isinstance(inputs, dict):
            raise PlannerError("invalid_llm_response", "LLM response inputs must be a JSON object.")

        step_inputs = self._build_step_inputs(
            command_id=command_id,
            schema_id=schema_id,
            inputs=inputs,
            planner_input=planner_input,
        )

        return {
            "command_id": command_id,
            "schema_id": schema_id,
            "inputs": step_inputs,
        }

    def _create_legacy_roughing_draft(self, planner_input: PlannerInput, parsed: Dict[str, object]) -> Dict[str, object]:
        fields = self._parse_flat_string_object(parsed)

        step_inputs = {
            "target_object_id": planner_input.target_object_id,
        }
        for field_name in REQUIRED_ROUGHING_FIELDS:
            if field_name in {"stepover", "stepdown", "tolerance"}:
                step_inputs[field_name] = self._normalize_positive_number(fields[field_name], field_name)
            else:
                step_inputs[field_name] = fields[field_name]
        step_inputs["tool_id"] = self._review_tool_choice(
            user_request=planner_input.user_request,
            proposed_tool_id=step_inputs["tool_id"],
        )

        return {
            "schema_version": "1",
            "trace_id": planner_input.trace_id,
            "status": "pending_review",
            "steps": [
                {
                    "command_id": "browser.createOperation",
                    "schema_id": "browser.createOperation.v1",
                    "inputs": step_inputs,
                }
            ],
            "trace_events": ["draft_created"],
        }

    @staticmethod
    def _parse_json_object(plan_json: str) -> Dict[str, object]:
        try:
            parsed = json.loads(plan_json)
        except json.JSONDecodeError as exc:
            raise PlannerError(
                "invalid_llm_response",
                "LLM response must be a JSON object.",
            ) from exc

        if not isinstance(parsed, dict):
            raise PlannerError(
                "invalid_llm_response",
                "LLM response must be a flat JSON object.",
            )
        return parsed

    @staticmethod
    def _parse_flat_string_object(parsed: Dict[str, object]) -> Dict[str, str]:
        fields: Dict[str, str] = {}
        missing: List[str] = []
        for field_name in REQUIRED_ROUGHING_FIELDS:
            value = parsed.get(field_name)
            if not isinstance(value, str) or not value:
                missing.append(field_name)
            else:
                fields[field_name] = value

        if missing:
            raise PlannerError(
                "missing_llm_field",
                "LLM response missing required fields: " + ", ".join(missing),
            )

        return fields

    def _build_step_inputs(
        self,
        *,
        command_id: str,
        schema_id: str,
        inputs: Dict[str, object],
        planner_input: PlannerInput,
    ) -> Dict[str, str]:
        if command_id == "browser.createOperation":
            if schema_id != "browser.createOperation.v1":
                raise PlannerError("unsupported_schema", "Unsupported schema_id for browser.createOperation.")
            return self._build_create_operation_inputs(inputs, planner_input)

        if command_id == "browser.updateOperation":
            if schema_id != "browser.updateOperation.v1":
                raise PlannerError("unsupported_schema", "Unsupported schema_id for browser.updateOperation.")
            return self._build_update_operation_inputs(inputs, planner_input)

        if command_id == "browser.generateToolpath":
            if schema_id != "browser.generateToolpath.v1":
                raise PlannerError("unsupported_schema", "Unsupported schema_id for browser.generateToolpath.")
            return {"target_object_id": planner_input.target_object_id}

        if command_id == "browser.setToolpathVisibility":
            if schema_id != "browser.setToolpathVisibility.v1":
                raise PlannerError("unsupported_schema", "Unsupported schema_id for browser.setToolpathVisibility.")
            return self._build_toolpath_visibility_inputs(inputs, planner_input)

        raise PlannerError("unsupported_command", "Unsupported command_id in LLM response: " + command_id)

    def _build_create_operation_inputs(self, inputs: Dict[str, object], planner_input: PlannerInput) -> Dict[str, str]:
        operation_type = self._operation_type_for_feature(
            proposed_operation_type=self._required_string(inputs, "operation_type"),
            planner_input=planner_input,
        )
        result = {
            "target_object_id": planner_input.target_object_id,
            "operation_type": operation_type,
            "tool_id": self._review_tool_choice(
                user_request=planner_input.user_request,
                operation_type=operation_type,
                proposed_tool_id=self._required_string(inputs, "tool_id"),
            ),
            "stepover": self._normalize_stepover_for_operation(
                value=self._required_string(inputs, "stepover"),
                operation_type=operation_type,
            ),
            "stepdown": self._normalize_positive_number(self._required_string(inputs, "stepdown"), "stepdown"),
            "tolerance": self._normalize_positive_number(self._required_string(inputs, "tolerance"), "tolerance"),
            "batch_count": self._normalize_positive_integer(str(inputs.get("batch_count", "1")), "batch_count"),
            "auto_generate_toolpath": self._auto_generate_toolpath_flag(inputs, planner_input),
        }
        return result

    def _build_update_operation_inputs(self, inputs: Dict[str, object], planner_input: PlannerInput) -> Dict[str, str]:
        return {
            "target_object_id": planner_input.target_object_id,
            "parameter_name": self._required_string(inputs, "parameter_name"),
            "parameter_value": self._required_string(inputs, "parameter_value"),
            "recompute_toolpath": self._boolean_string(str(inputs.get("recompute_toolpath", "false"))),
        }

    def _build_toolpath_visibility_inputs(self, inputs: Dict[str, object], planner_input: PlannerInput) -> Dict[str, str]:
        visibility = self._required_string(inputs, "visibility")
        explicit_operation_type = str(inputs.get("operation_type", ""))
        operation_type = explicit_operation_type or self._operation_type_filter_for_visibility(planner_input.user_request)
        scope = "operation_type" if operation_type else self._required_string(inputs, "scope")
        if visibility not in {"show", "hide", "toggle"}:
            raise PlannerError("invalid_llm_response", "Toolpath visibility must be show, hide, or toggle.")
        if scope not in {"all", "selected", "list", "operation_type"}:
            raise PlannerError("invalid_llm_response", "Toolpath visibility scope must be all, selected, list, or operation_type.")
        return {
            "target_object_id": planner_input.target_object_id,
            "visibility": visibility,
            "scope": scope,
            "toolpath_ids": "" if operation_type else str(inputs.get("toolpath_ids", "")),
            "operation_type": operation_type or str(inputs.get("operation_type", "")),
        }

    @staticmethod
    def _required_string(parsed: Dict[str, object], name: str) -> str:
        value = parsed.get(name)
        if not isinstance(value, str) or not value:
            raise PlannerError("missing_llm_field", "LLM response missing required field: " + name)
        return value

    @staticmethod
    def _normalize_positive_number(value: str, field_name: str) -> str:
        match = re.search(r"[-+]?\d+(?:\.\d+)?", value)
        if not match:
            raise PlannerError(
                "invalid_numeric_field",
                f"LLM response field {field_name} must contain a positive numeric value.",
            )
        parsed = float(match.group(0))
        if parsed <= 0:
            raise PlannerError(
                "invalid_numeric_field",
                f"LLM response field {field_name} must be positive.",
            )
        if parsed.is_integer():
            return str(int(parsed))
        return ("%f" % parsed).rstrip("0").rstrip(".")

    @staticmethod
    def _normalize_non_negative_number(value: str, field_name: str) -> str:
        match = re.search(r"[-+]?\d+(?:\.\d+)?", value)
        if not match:
            raise PlannerError(
                "invalid_numeric_field",
                f"LLM response field {field_name} must contain a numeric value.",
            )
        parsed = float(match.group(0))
        if parsed < 0:
            raise PlannerError(
                "invalid_numeric_field",
                f"LLM response field {field_name} must not be negative.",
            )
        if parsed.is_integer():
            return str(int(parsed))
        return ("%f" % parsed).rstrip("0").rstrip(".")

    @staticmethod
    def _normalize_positive_integer(value: str, field_name: str) -> str:
        match = re.search(r"\d+", value)
        if not match:
            raise PlannerError("invalid_numeric_field", f"LLM response field {field_name} must contain a positive integer.")
        parsed = int(match.group(0))
        if parsed <= 0:
            raise PlannerError("invalid_numeric_field", f"LLM response field {field_name} must be positive.")
        return str(parsed)

    @staticmethod
    def _boolean_string(value: str) -> str:
        lowered = value.strip().lower()
        return "true" if lowered in {"true", "1", "yes", "y", "是", "需要"} else "false"

    def _review_tool_choice(self, *, user_request: str, operation_type: str = "", proposed_tool_id: str) -> str:
        requested_diameter = self._requested_tool_diameter_mm(user_request)
        if requested_diameter is not None:
            return self._tool_library.best_tool_id_at_or_below(requested_diameter)
        if self._tool_library.has_tool_id(proposed_tool_id):
            return proposed_tool_id
        return self._tool_library.default_tool_id_for_operation(operation_type)

    def _operation_type_for_feature(self, *, proposed_operation_type: str, planner_input: PlannerInput) -> str:
        if self._request_mentions_operation_type(planner_input.user_request, proposed_operation_type):
            return proposed_operation_type
        feature_text = planner_input.target_display_name + " " + planner_input.target_object_id + " " + planner_input.user_request
        recognized = self._feature_recognizer.recognize_operation_type(feature_text)
        if recognized.operation_type:
            return recognized.operation_type
        return proposed_operation_type

    @staticmethod
    def _request_mentions_operation_type(user_request: str, operation_type: str) -> bool:
        lowered = user_request.lower()
        keywords_by_type = {
            "drilling": ("钻孔", "孔", "drilling", "drill"),
            "pocket": ("型腔铣", "型腔", "pocket", "roughing"),
            "roughing": ("型腔铣", "型腔", "pocket", "roughing"),
            "pocket_roughing": ("型腔铣", "型腔", "pocket", "roughing"),
            "finishing": ("平面铣", "平面", "finishing", "plane"),
        }
        return any(keyword in lowered for keyword in keywords_by_type.get(operation_type, (operation_type,)))

    def _normalize_stepover_for_operation(self, *, value: str, operation_type: str) -> str:
        if operation_type == "drilling":
            return self._normalize_non_negative_number(value, "stepover")
        return self._normalize_positive_number(value, "stepover")

    @staticmethod
    def _auto_generate_toolpath_flag(inputs: Dict[str, object], planner_input: PlannerInput) -> str:
        explicit = inputs.get("auto_generate_toolpath")
        if explicit is not None:
            return AgentPlanner._boolean_string(str(explicit))

        request = planner_input.user_request.lower()
        toolpath_words = ("加工", "生成刀轨", "计算刀轨", "刀轨", "machine", "process", "calculate", "toolpath")
        create_only_words = ("创建", "create")
        batch_count = str(inputs.get("batch_count", "1"))
        if any(word in request for word in toolpath_words):
            return "true"
        if any(word in request for word in create_only_words) and batch_count != "1":
            return "false"
        return "false"

    @staticmethod
    def _requested_tool_diameter_mm(user_request: str) -> float | None:
        patterns = [
            r"\b[dD]\s*=?\s*(\d+(?:\.\d+)?)\s*mm?",
            r"直径\s*(\d+(?:\.\d+)?)\s*mm?",
            r"(\d+(?:\.\d+)?)\s*mm\s*的?刀",
        ]
        for pattern in patterns:
            match = re.search(pattern, user_request)
            if match:
                return float(match.group(1))
        return None

    @staticmethod
    def _operation_type_filter_for_visibility(user_request: str) -> str:
        operation_types = AgentPlanner._operation_type_filters_for_visibility(user_request)
        return operation_types[0] if operation_types else ""

    @staticmethod
    def _operation_type_filters_for_visibility(user_request: str) -> List[str]:
        lowered = user_request.lower()
        keyword_groups = [
            ("pocket", ("型腔铣", "型腔", "pocket", "roughing")),
            ("drilling", ("钻孔", "孔", "drilling", "drill")),
            ("finishing", ("平面铣", "平面", "finishing", "plane")),
        ]
        matches = []
        for operation_type, keywords in keyword_groups:
            positions = [lowered.find(keyword) for keyword in keywords if lowered.find(keyword) >= 0]
            if positions:
                matches.append((min(positions), operation_type))
        matches.sort(key=lambda item: item[0])
        return [operation_type for _, operation_type in matches]
