# CAMClaw Agent Instructions

## Project constraints

- C++ implementation should primarily use C++11. C++14 is allowed where it clearly improves the implementation, but do not use C++17 or newer features.
- Do not implement prompt keyword matching in product code. Prompts, docs, examples, or tests may show natural-language use cases to teach the model what to do, but runtime code must not branch by matching user prompt strings such as fixed keywords or phrases. Natural-language intent should be turned into structured plans, commands, and validated arguments before execution.

## Agent skills

### Issue tracker

Issues and PRDs for this repo live in GitHub Issues for `HUYINHAO999/CAMClaw`. See `docs/agents/issue-tracker.md`.

### Triage labels

Triage uses concise Chinese labels mapped from the canonical skill roles. See `docs/agents/triage-labels.md`.

### Domain docs

This is a single-context repo: read `CONTEXT.md` and relevant ADRs under `docs/adr/`. See `docs/agents/domain.md`.
