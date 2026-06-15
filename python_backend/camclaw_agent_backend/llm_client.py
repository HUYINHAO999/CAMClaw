from __future__ import annotations

import json
import os
import urllib.error
import urllib.request
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Dict
from urllib.parse import urlparse


@dataclass(frozen=True)
class LlmConfig:
    base_url: str
    api_key: str
    model: str

    @staticmethod
    def from_env() -> "LlmConfig":
        return LlmConfig.from_file().with_environment_overrides()

    @staticmethod
    def from_file(config_path: Path | None = None) -> "LlmConfig":
        path = config_path or Path(__file__).resolve().parents[1] / "config" / "agent_backend.json"
        if not path.is_file():
            return LlmConfig(base_url="", api_key="", model="")

        with path.open("r", encoding="utf-8") as config_file:
            config = json.load(config_file)

        llm_config = config.get("llm", {})
        return LlmConfig(
            base_url=str(llm_config.get("base_url", "")),
            api_key=str(llm_config.get("api_key", "")),
            model=str(llm_config.get("model", "")),
        )

    def with_environment_overrides(self) -> "LlmConfig":
        return LlmConfig(
            base_url=os.environ.get("CAMCLAW_LLM_BASE_URL", self.base_url),
            api_key=os.environ.get("CAMCLAW_LLM_API_KEY", self.api_key),
            model=os.environ.get("CAMCLAW_LLM_MODEL", self.model),
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
        base_url = self._config.base_url.rstrip("/")
        parsed = urlparse(base_url)
        if parsed.scheme not in ("http", "https") or not parsed.netloc:
            raise LlmClientError("CAMCLAW_LLM_BASE_URL must be an absolute http(s) URL.")
        return base_url + "/v1/chat/completions"

    @staticmethod
    def _extract_message_content(response: Dict[str, Any]) -> str:
        try:
            return response["choices"][0]["message"]["content"]
        except (KeyError, IndexError, TypeError) as exc:
            raise LlmClientError("LLM response missing choices[0].message.content") from exc
