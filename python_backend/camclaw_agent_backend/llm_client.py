from __future__ import annotations

import json
import os
import urllib.error
import urllib.request
from dataclasses import dataclass
from typing import Any, Dict


@dataclass(frozen=True)
class LlmConfig:
    base_url: str
    api_key: str
    model: str

    @staticmethod
    def from_env() -> "LlmConfig":
        return LlmConfig(
            base_url=os.environ.get("CAMCLAW_LLM_BASE_URL", ""),
            api_key=os.environ.get("CAMCLAW_LLM_API_KEY", ""),
            model=os.environ.get("CAMCLAW_LLM_MODEL", "gpt-5.5"),
        )


class LlmClientError(RuntimeError):
    pass


class OpenAICompatibleClient:
    def __init__(self, config: LlmConfig):
        self._config = config

    def create_plan_json(
        self,
        *,
        trace_id: str,
        user_request: str,
        target_object_id: str,
        rejection_reason: str,
        response_contract: str,
    ) -> str:
        payload = {
            "model": self._config.model,
            "messages": [
                {
                    "role": "system",
                    "content": (
                        "You produce CAM AgentPlanDraft inputs. "
                        + response_contract
                    ),
                },
                {
                    "role": "user",
                    "content": {
                        "trace_id": trace_id,
                        "target_object_id": target_object_id,
                        "user_request": user_request,
                        "rejection_reason": rejection_reason,
                    },
                },
            ],
        }

        request = urllib.request.Request(
            self._chat_completions_url(),
            data=json.dumps(payload, ensure_ascii=False).encode("utf-8"),
            headers={
                "Authorization": f"Bearer {self._config.api_key}",
                "Content-Type": "application/json",
            },
            method="POST",
        )

        try:
            with urllib.request.urlopen(request, timeout=30) as response:
                body = response.read().decode("utf-8")
        except urllib.error.URLError as exc:
            raise LlmClientError(str(exc)) from exc

        return self._extract_message_content(json.loads(body))

    def _chat_completions_url(self) -> str:
        return self._config.base_url.rstrip("/") + "/v1/chat/completions"

    @staticmethod
    def _extract_message_content(response: Dict[str, Any]) -> str:
        try:
            return response["choices"][0]["message"]["content"]
        except (KeyError, IndexError, TypeError) as exc:
            raise LlmClientError("LLM response missing choices[0].message.content") from exc
