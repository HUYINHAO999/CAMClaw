import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from camclaw_agent_backend.llm_client import LlmClientError, LlmConfig, OpenAICompatibleClient


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
                rejection_reason="只要粗加工，不要精加工",
                response_contract="Return JSON",
            )

        self.assertEqual("https://llm.example/v1/chat/completions", captured["url"])
        self.assertEqual("Bearer local-secret", captured["headers"]["Authorization"])
        self.assertEqual("application/json", captured["headers"]["Content-type"])
        self.assertEqual("gpt-5.5", captured["body"]["model"])
        self.assertEqual(
            "只要粗加工，不要精加工",
            captured["body"]["messages"][1]["content"]["rejection_reason"],
        )
        self.assertEqual(30, captured["timeout"])
        self.assertIn('"operation_type":"roughing"', content)

    def test_default_config_uses_project_llm_endpoint(self):
        with patch.dict("os.environ", {}, clear=True):
            config = LlmConfig.from_env()

        self.assertEqual("https://jspin.cn", config.base_url)
        self.assertEqual("gpt-5.5", config.model)
        self.assertEqual("", config.api_key)

    def test_reads_endpoint_from_config_file(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            config_path = Path(temp_dir) / "agent_backend.json"
            config_path.write_text(
                json.dumps(
                    {
                        "llm": {
                            "base_url": "https://configured.example",
                            "model": "configured-model",
                        }
                    }
                ),
                encoding="utf-8",
            )

            config = LlmConfig.from_file(config_path)

        self.assertEqual("https://configured.example", config.base_url)
        self.assertEqual("configured-model", config.model)

    def test_environment_overrides_config_file(self):
        config = LlmConfig(
            base_url="https://configured.example",
            api_key="",
            model="configured-model",
        )

        with patch.dict(
            "os.environ",
            {
                "CAMCLAW_LLM_BASE_URL": "https://env.example",
                "CAMCLAW_LLM_API_KEY": "local-secret",
                "CAMCLAW_LLM_MODEL": "env-model",
            },
            clear=True,
        ):
            overridden = config.with_environment_overrides()

        self.assertEqual("https://env.example", overridden.base_url)
        self.assertEqual("local-secret", overridden.api_key)
        self.assertEqual("env-model", overridden.model)

    def test_rejects_relative_base_url_before_http_call(self):
        client = OpenAICompatibleClient(
            LlmConfig(
                base_url="",
                api_key="local-secret",
                model="gpt-5.5",
            )
        )

        with self.assertRaises(LlmClientError) as caught:
            client.create_plan_json(
                trace_id="trace_http_002",
                user_request="给当前型腔做粗加工",
                target_object_id="feature_001",
                rejection_reason="",
                response_contract="Return JSON",
            )

        self.assertIn("absolute http(s) URL", str(caught.exception))


if __name__ == "__main__":
    unittest.main()
