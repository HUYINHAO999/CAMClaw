# Domain Docs

How the engineering skills should consume this repo's domain documentation when exploring the codebase.

## Layout

CAMClaw is a single-context repo.

Before exploring or planning a change, read:

- `CONTEXT.md` at the repo root
- Relevant ADRs under `docs/adr/`

If an ADR directory or relevant ADR does not exist yet, proceed silently.

## Use the glossary's vocabulary

When your output names a domain concept in an issue title, refactor proposal, hypothesis, test name, or implementation plan, use the term as defined in `CONTEXT.md`.

If the concept you need is not in the glossary yet, either avoid inventing new language or note the gap for a future documentation update.

## Flag ADR conflicts

If your output contradicts an existing ADR, surface it explicitly rather than silently overriding it.
