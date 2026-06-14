from __future__ import annotations

import json
from dataclasses import dataclass
from typing import Dict, List, Protocol

from .contracts import REQUIRED_ROUGHING_FIELDS, roughing_plan_json_contract


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


class PlannerError(ValueError):
    def __init__(self, error_code: str, message: str):
        super().__init__(message)
        self.error_code = error_code
        self.message = message


class AgentPlanner:
    def __init__(self, llm_client: LlmPlanningClient):
        self._llm_client = llm_client

    def create_draft(self, planner_input: PlannerInput) -> Dict[str, object]:
        plan_json = self._llm_client.create_plan_json(
            trace_id=planner_input.trace_id,
            user_request=planner_input.user_request,
            target_object_id=planner_input.target_object_id,
            rejection_reason=planner_input.rejection_reason,
            response_contract=roughing_plan_json_contract(),
        )
        fields = self._parse_flat_string_object(plan_json)

        step_inputs = {
            "target_object_id": planner_input.target_object_id,
        }
        for field_name in REQUIRED_ROUGHING_FIELDS:
            step_inputs[field_name] = fields[field_name]

        return {
            "trace_id": planner_input.trace_id,
            "status": "pending_review",
            "steps": [
                {
                    "skill_id": "browser.create_roughing_operation_and_generate_toolpath",
                    "inputs": step_inputs,
                }
            ],
            "trace_events": ["draft_created"],
        }

    @staticmethod
    def _parse_flat_string_object(plan_json: str) -> Dict[str, str]:
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
