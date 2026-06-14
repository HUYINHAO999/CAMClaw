import unittest

from camclaw_agent_backend.planner import AgentPlanner, PlannerError, PlannerInput


class FakeLlmClient:
    def __init__(self, response):
        self.response = response
        self.last_request = None

    def create_plan_json(self, **kwargs):
        self.last_request = kwargs
        return self.response


class AgentPlannerTests(unittest.TestCase):
    def test_builds_draft_from_structured_llm_response(self):
        llm = FakeLlmClient(
            "{"
            '"operation_type":"roughing",'
            '"tool_id":"tool_010",'
            '"stepover":"2.0",'
            '"stepdown":"1.0",'
            '"tolerance":"0.02"'
            "}"
        )
        planner = AgentPlanner(llm)

        draft = planner.create_draft(
            PlannerInput(
                trace_id="trace_py_001",
                user_request="给当前型腔做粗加工",
                target_object_id="feature_001",
            )
        )

        self.assertEqual("trace_py_001", draft["trace_id"])
        self.assertEqual("pending_review", draft["status"])
        self.assertEqual(
            "browser.create_roughing_operation_and_generate_toolpath",
            draft["steps"][0]["skill_id"],
        )
        self.assertEqual("feature_001", draft["steps"][0]["inputs"]["target_object_id"])
        self.assertEqual("tool_010", draft["steps"][0]["inputs"]["tool_id"])
        self.assertIn("operation_type", llm.last_request["response_contract"])

    def test_rejects_unstructured_llm_response(self):
        planner = AgentPlanner(FakeLlmClient("roughing with tool_010"))

        with self.assertRaises(PlannerError) as caught:
            planner.create_draft(
                PlannerInput(
                    trace_id="trace_py_002",
                    user_request="给当前型腔做粗加工",
                    target_object_id="feature_001",
                )
            )

        self.assertEqual("invalid_llm_response", caught.exception.error_code)

    def test_rejects_missing_required_llm_fields(self):
        planner = AgentPlanner(FakeLlmClient('{"operation_type":"roughing"}'))

        with self.assertRaises(PlannerError) as caught:
            planner.create_draft(
                PlannerInput(
                    trace_id="trace_py_003",
                    user_request="给当前型腔做粗加工",
                    target_object_id="feature_001",
                )
            )

        self.assertEqual("missing_llm_field", caught.exception.error_code)


if __name__ == "__main__":
    unittest.main()
