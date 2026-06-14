from __future__ import annotations

import json
import mimetypes
from pathlib import Path
from http.server import BaseHTTPRequestHandler, HTTPServer
from typing import Any, Dict

from .llm_client import LlmConfig, OpenAICompatibleClient
from .planner import AgentPlanner, PlannerError, PlannerInput


class AgentBackendHandler(BaseHTTPRequestHandler):
    planner = AgentPlanner(OpenAICompatibleClient(LlmConfig.from_env()))
    ui_root = Path(__file__).resolve().parents[2] / "ui_prototype" / "agent_review"

    def do_GET(self) -> None:
        if self.path == "/":
            self._redirect("/agent-review/")
            return

        if self.path == "/agent-review":
            self._redirect("/agent-review/")
            return

        if self.path == "/agent-review/":
            self._send_static_file(self.ui_root / "index.html")
            return

        if self.path.startswith("/agent-review/"):
            relative_path = self.path[len("/agent-review/") :]
            self._send_static_file(self.ui_root / relative_path)
            return

        self._send_json(404, {"error_code": "not_found"})

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
                    rejection_reason=str(payload.get("rejection_reason", "")),
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

    def _send_static_file(self, path: Path) -> None:
        resolved_root = self.ui_root.resolve()
        resolved_path = path.resolve()
        if resolved_root not in resolved_path.parents and resolved_path != resolved_root:
            self._send_json(403, {"error_code": "forbidden"})
            return

        if not resolved_path.is_file():
            self._send_json(404, {"error_code": "not_found"})
            return

        content_type = mimetypes.guess_type(str(resolved_path))[0] or "application/octet-stream"
        body = resolved_path.read_bytes()
        self.send_response(200)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

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

    def _redirect(self, location: str) -> None:
        body = b""
        self.send_response(302)
        self.send_header("Location", location)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()


def run(host: str = "127.0.0.1", port: int = 8765) -> None:
    HTTPServer((host, port), AgentBackendHandler).serve_forever()


if __name__ == "__main__":
    run()
