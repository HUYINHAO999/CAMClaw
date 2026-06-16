from __future__ import annotations

from typing import Any, Dict, List, Literal, Optional, Union

from pydantic import BaseModel, ConfigDict, Field, RootModel, field_validator, model_validator


OperationType = Literal["pocket", "drilling", "finishing", "roughing", "pocket_roughing"]
ObjectType = Literal["feature", "operation", "toolpath"]
Visibility = Literal["show", "hide", "toggle"]


class AgentPlanRequestModel(BaseModel):
    model_config = ConfigDict(extra="forbid")

    trace_id: str
    user_request: str
    target_object_id: str = ""
    target_display_name: str = ""
    rejection_reason: str = ""
    selected_object_ids: List[str] = Field(default_factory=list)
    context: Dict[str, Any] = Field(default_factory=dict)

    @field_validator("trace_id", "user_request")
    @classmethod
    def require_non_empty(cls, value: str) -> str:
        if not value.strip():
            raise ValueError("field must be non-empty")
        return value


class ObjectsTarget(BaseModel):
    model_config = ConfigDict(extra="forbid")

    kind: Literal["objects"]
    object_ids: List[str]

    @field_validator("object_ids")
    @classmethod
    def require_object_ids(cls, value: List[str]) -> List[str]:
        if not value:
            raise ValueError("object_ids must be non-empty")
        return value


class QueryTarget(BaseModel):
    model_config = ConfigDict(extra="forbid")

    kind: Literal["query"]
    object_type: ObjectType
    scope: Literal["all", "matching"]
    filters: Dict[str, str] = Field(default_factory=dict)

    @model_validator(mode="after")
    def validate_filters(self) -> "QueryTarget":
        if self.scope == "matching" and not self.filters:
            raise ValueError("matching query target requires filters")
        return self


class RefTarget(BaseModel):
    model_config = ConfigDict(extra="forbid")

    kind: Literal["ref"]
    ref: Literal["previous.primary_object_ids"]


TargetSelector = Union[ObjectsTarget, QueryTarget, RefTarget]


class OperationCreateItem(BaseModel):
    model_config = ConfigDict(extra="forbid")

    operation_type: OperationType
    count: int = 1

    @field_validator("count")
    @classmethod
    def require_positive_count(cls, value: int) -> int:
        if value <= 0:
            raise ValueError("count must be positive")
        return value


class ParameterUpdate(BaseModel):
    model_config = ConfigDict(extra="forbid")

    parameter: Literal["tool", "tool_id", "stepover", "stepdown", "tolerance"]
    value: Optional[str] = None
    expression: Optional[str] = None

    @model_validator(mode="after")
    def require_value_or_expression(self) -> "ParameterUpdate":
        if not self.value and not self.expression:
            raise ValueError("parameter update requires value or expression")
        if self.value and self.expression:
            raise ValueError("parameter update cannot contain both value and expression")
        return self


class VisibilityAction(BaseModel):
    model_config = ConfigDict(extra="forbid")

    visibility: Visibility
    target: TargetSelector


class CreateOperationsIntent(BaseModel):
    model_config = ConfigDict(extra="forbid")

    id: str
    intent: Literal["create_operations"]
    items: List[OperationCreateItem]
    target: Optional[TargetSelector] = None
    auto_generate_toolpath: bool = False

    @field_validator("items")
    @classmethod
    def require_items(cls, value: List[OperationCreateItem]) -> List[OperationCreateItem]:
        if not value:
            raise ValueError("create_operations requires at least one item")
        return value


class MachineFeatureIntent(BaseModel):
    model_config = ConfigDict(extra="forbid")

    id: str
    intent: Literal["machine_feature"]
    target: TargetSelector
    auto_generate_toolpath: bool = True
    requirements: Dict[str, str] = Field(default_factory=dict)


class EditOperationIntent(BaseModel):
    model_config = ConfigDict(extra="forbid")

    id: str
    intent: Literal["edit_operation"]
    target: TargetSelector
    updates: List[ParameterUpdate]
    open_editor: bool = False

    @field_validator("updates")
    @classmethod
    def require_updates(cls, value: List[ParameterUpdate]) -> List[ParameterUpdate]:
        if not value:
            raise ValueError("edit_operation requires at least one update")
        return value


class RegenerateToolpathIntent(BaseModel):
    model_config = ConfigDict(extra="forbid")

    id: str
    intent: Literal["regenerate_toolpath"]
    target: TargetSelector


class SetToolpathVisibilityIntent(BaseModel):
    model_config = ConfigDict(extra="forbid")

    id: str
    intent: Literal["set_toolpath_visibility"]
    actions: List[VisibilityAction]

    @field_validator("actions")
    @classmethod
    def require_actions(cls, value: List[VisibilityAction]) -> List[VisibilityAction]:
        if not value:
            raise ValueError("set_toolpath_visibility requires at least one action")
        return value


SemanticIntent = Union[
    CreateOperationsIntent,
    MachineFeatureIntent,
    EditOperationIntent,
    RegenerateToolpathIntent,
    SetToolpathVisibilityIntent,
]


class SemanticIntentPlan(BaseModel):
    model_config = ConfigDict(extra="forbid")

    schema_version: Literal["1"]
    intents: List[SemanticIntent]

    @field_validator("intents")
    @classmethod
    def require_intents(cls, value: List[SemanticIntent]) -> List[SemanticIntent]:
        if not value:
            raise ValueError("semantic plan requires at least one intent")
        return value


class SemanticIntentPlanRoot(RootModel[SemanticIntentPlan]):
    pass
