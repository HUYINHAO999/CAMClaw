from __future__ import annotations

import json
from http.server import BaseHTTPRequestHandler, HTTPServer
from typing import Any, Dict

from .llm_client import LlmConfig, OpenAICompatibleClient
from .planner import AgentPlanner, PlannerError, PlannerInput


class AgentBackendHandler(BaseHTTPRequestHandler):
    planner = AgentPlanner(OpenAICompatibleClient(LlmConfig.from_env()))

    def do_POST(self) -> None:
        if self.path != "/v1/agent/plan":
            self._send_json(404, {"error_code": "not_found"})
            return

        try:
            payload = self._read_json()
            draft = self.planner.create_draft(
                PlannerInput(
                    trace_id=str(payload["trace_id"]),
                    user_request=str(payload["user_request"]),
                    target_object_id=str(payload["target_object_id"]),
                )
            )
        except KeyError as exc:
            self._send_json(400, {"error_code": "missing_field", "message": str(exc)})
            return
        except PlannerError as exc:
            self._send_json(422, {"error_code": exc.error_code, "message": exc.message})
            return
        except Exception as exc:
            self._send_json(500, {"error_code": "planner_failed", "message": str(exc)})
            return

        self._send_json(200, draft)

    def _read_json(self) -> Dict[str, Any]:
        content_length = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(content_length).decode("utf-8")
        parsed = json.loads(body)
        if not isinstance(parsed, dict):
            raise ValueError("request body must be a JSON object")
        return parsed

    def _send_json(self, status_code: int, payload: Dict[str, Any]) -> None:
        body = json.dumps(payload, ensure_ascii=False).encode("utf-8")
        self.send_response(status_code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)


def run(host: str = "127.0.0.1", port: int = 8765) -> None:
    HTTPServer((host, port), AgentBackendHandler).serve_forever()


if __name__ == "__main__":
    run()
