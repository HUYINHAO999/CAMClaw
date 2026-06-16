import unittest

from camclaw_agent_backend.server import AgentBackendHandler


class ServerStaticUiTests(unittest.TestCase):
    def test_ui_root_exists(self):
        index_html = AgentBackendHandler.ui_root / "index.html"

        self.assertTrue(index_html.is_file())
        self.assertIn("Agent 草案审核", index_html.read_text(encoding="utf-8"))

    def test_request_path_ignores_query_string(self):
        self.assertEqual(
            "/agent-review/",
            AgentBackendHandler._request_path("/agent-review/?check=3"),
        )
        self.assertEqual(
            "/v1/agent/plan",
            AgentBackendHandler._request_path("/v1/agent/plan?trace=manual"),
        )


if __name__ == "__main__":
    unittest.main()
