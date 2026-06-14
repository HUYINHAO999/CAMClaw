# Issue tracker: GitHub

Issues and PRDs for this repo live as GitHub issues in `HUYINHAO999/CAMClaw`.

Repository URL:

```text
https://github.com/HUYINHAO999/CAMClaw
```

## Conventions

When the GitHub CLI is installed and authenticated, use `gh` for issue operations:

- **Create an issue**: `gh issue create --title "..." --body "..."`
- **Read an issue**: `gh issue view <number> --comments`
- **List issues**: `gh issue list --state open --json number,title,body,labels,comments`
- **Comment on an issue**: `gh issue comment <number> --body "..."`
- **Apply / remove labels**: `gh issue edit <number> --add-label "..."` / `--remove-label "..."`
- **Close**: `gh issue close <number> --comment "..."`

Infer the repo from `git remote -v`; `gh` does this automatically when run inside a clone.

## Current machine note

The current machine does not have the `gh` CLI available. Until it is installed and authenticated, use the GitHub web UI for creating the repository, issues, labels, and PRDs.

## When a skill says "publish to the issue tracker"

Create a GitHub issue in `HUYINHAO999/CAMClaw`.

## When a skill says "fetch the relevant ticket"

Use `gh issue view <number> --comments` if `gh` is available. Otherwise, open the issue in the GitHub web UI and read the issue body, comments, and labels.
