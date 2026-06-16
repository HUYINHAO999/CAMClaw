from __future__ import annotations

import json
from dataclasses import dataclass
from typing import Dict, Protocol

from pydantic import ValidationError

from .contracts import semantic_intent_plan_json_contract
from .semantic_schemas import SemanticIntentPlan
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
    ):
        self._llm_client = llm_client
        self._tool_library = tool_library or ToolLibrary.from_file()

    def create_draft(self, planner_input: PlannerInput) -> Dict[str, object]:
        plan_json = self._llm_client.create_plan_json(
            trace_id=planner_input.trace_id,
            user_request=planner_input.user_request,
            target_object_id=planner_input.target_object_id,
            rejection_reason=planner_input.rejection_reason,
            response_contract=semantic_intent_plan_json_contract(self._tool_library.to_planning_context()),
        )
        parsed = self._parse_json_object(plan_json)
        try:
            plan = SemanticIntentPlan.model_validate(parsed)
        except ValidationError as exc:
            raise PlannerError("invalid_semantic_intent", str(exc)) from exc
        return self._build_semantic_draft_response(planner_input, plan)

    @staticmethod
    def _parse_json_object(plan_json: str) -> Dict[str, object]:
        try:
            parsed = json.loads(plan_json)
        except json.JSONDecodeError as exc:
            raise PlannerError("invalid_llm_response", "LLM response must be a JSON object.") from exc
        if not isinstance(parsed, dict):
            raise PlannerError("invalid_llm_response", "LLM response must be a JSON object.")
        return parsed

    @staticmethod
    def _build_semantic_draft_response(planner_input: PlannerInput, plan: SemanticIntentPlan) -> Dict[str, object]:
        data = plan.model_dump(mode="json")
        data["trace_id"] = planner_input.trace_id
        data["status"] = "pending_review"
        data["trace_events"] = ["semantic_draft_created"]
        return data
