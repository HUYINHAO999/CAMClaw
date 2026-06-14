# Agent Review Manual Test

## Start Backend

Use PowerShell from the repo root:

```powershell
$env:CAMCLAW_LLM_API_KEY="replace-with-local-secret"
.\scripts\run_agent_review_backend.ps1
```

The script defaults to:

```text
CAMCLAW_LLM_BASE_URL=https://jspin.cn
CAMCLAW_LLM_MODEL=gpt-5.5
```

## Open UI

```text
http://127.0.0.1:8765/agent-review/
```

## Smoke Test

1. Confirm the page shows target object `型腔 A`.
2. Click `生成草案`.
3. Review operation type, tool, stepover, stepdown, and tolerance.
4. Edit one parameter value.
5. Click `确认执行`.
6. Confirm execution result JSON contains:
   - `ok: true`
   - `op_roughing_feature_001`
   - `toolpath_op_roughing_feature_001`
7. Reload the page.
8. Click `拒绝草案`.
9. Confirm the rejection reason is shown in execution result JSON.

## Current Prototype Limits

- The UI executes against an in-browser simulation, not the real C++ binary.
- C++ already has tests proving decoded backend draft JSON can execute through the C++ core.
- Real Qt integration should replace this browser prototype with Qt widgets or QML while keeping the same states and JSON boundary.
