import json
import unittest
from unittest.mock import patch

from camclaw_agent_backend.llm_client import LlmConfig, OpenAICompatibleClient


class FakeResponse:
    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc, tb):
        return False

    def read(self):
        return json.dumps(
            {
                "choices": [
                    {
                        "message": {
                            "content": '{"operation_type":"roughing","tool_id":"tool_010","stepover":"2.0","stepdown":"1.0","tolerance":"0.02"}'
                        }
                    }
                ]
            }
        ).encode("utf-8")


class OpenAICompatibleClientTests(unittest.TestCase):
    def test_sends_openai_compatible_request(self):
        captured = {}

        def fake_urlopen(request, timeout):
            captured["url"] = request.full_url
            captured["headers"] = dict(request.header_items())
            captured["body"] = json.loads(request.data.decode("utf-8"))
            captured["timeout"] = timeout
            return FakeResponse()

        client = OpenAICompatibleClient(
            LlmConfig(
                base_url="https://llm.example",
                api_key="local-secret",
                model="gpt-5.5",
            )
        )

        with patch("urllib.request.urlopen", fake_urlopen):
            content = client.create_plan_json(
                trace_id="trace_http_001",
                user_request="给当前型腔做粗加工",
                target_object_id="feature_001",
                response_contract="Return JSON",
            )

        self.assertEqual("https://llm.example/v1/chat/completions", captured["url"])
        self.assertEqual("Bearer local-secret", captured["headers"]["Authorization"])
        self.assertEqual("application/json", captured["headers"]["Content-type"])
        self.assertEqual("gpt-5.5", captured["body"]["model"])
        self.assertEqual(30, captured["timeout"])
        self.assertIn('"operation_type":"roughing"', content)


if __name__ == "__main__":
    unittest.main()
