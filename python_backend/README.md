# CAMClaw Agent Python Backend

This backend owns LLM calls and natural-language planning.

The C++ core should not call the model directly. It should receive reviewed, structured artifacts such as `AgentPlanDraft` and execute them through Skill, Gateway, Adapter, Console, and Repository.

## Environment

Use local environment variables. Do not commit real secrets.

```text
CAMCLAW_LLM_BASE_URL=https://jspin.cn
CAMCLAW_LLM_API_KEY=replace-with-local-secret
CAMCLAW_LLM_MODEL=gpt-5.5
```

## Run Tests

```powershell
$env:PYTHONPATH="F:\CAMClaw\python_backend"
python -m unittest discover -s python_backend\tests
```

## Run Server

```powershell
$env:PYTHONPATH="F:\CAMClaw\python_backend"
python -m camclaw_agent_backend.server
```

Endpoint:

```text
POST /v1/agent/plan
```

UI prototype:

```text
http://127.0.0.1:8765/agent-review/
```
