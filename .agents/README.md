# `.agents/` — tool-independent F´ skills for AI agents

This directory holds the **canonical** prompts/skills/playbooks that guide AI
coding agents working in this repository (and, eventually, `nasa/fprime`). They
are plain Markdown, intentionally tool-agnostic, and referenced by every AI
surface in this repo:

- The repo-root [`AGENTS.md`](../AGENTS.md) links here as the skill catalog.
- VS Code Copilot custom agents in [`../.github/agents/`](../.github/agents) wrap
  these files with the YAML frontmatter VS Code needs and instruct the agent to
  load the canonical content from `.agents/`.
- Windsurf ([`../.windsurf/rules/`](../.windsurf/rules)) and Claude Code
  ([`../CLAUDE.md`](../CLAUDE.md)) read `AGENTS.md`, which in turn links here.

## Skill index

| File | When to use |
|---|---|
| [`code-review.md`](code-review.md) | Reviewing F´ C++ changes for policy compliance, safety, security, style, test coverage, SDD updates, and PR readiness. |
| [`component-author.md`](component-author.md) | Authoring a new F´ component end-to-end: FPP model → autocode → C++ impl → unit-test scaffold → SDD stub. |
| [`unit-test-author.md`](unit-test-author.md) | Writing or expanding unit tests for an existing F´ component using gtest + `STest`. |

## Conventions

1. **One concern per file.** Each skill targets a single well-scoped task. Long
   omnibus prompts dilute model behavior.
2. **No tool-specific syntax.** Don't reference Copilot chat variables, Claude
   slash commands, or any other tool-specific invocation. Tool wrappers handle
   that.
3. **Reference, don't duplicate.** Link to `CONTRIBUTING.md`, `AI_POLICY.md`,
   `.github/untrusted-pr-review-policy.md`, `AGENTS.md`, and the F´ user manual
   instead of restating their content.
4. **Imperative voice.** Skills tell the agent what to do, not what to consider.

## Adding a new skill

1. Add a `.agents/<skill-name>.md` file using the conventions above. Start with a
   one-sentence purpose statement, then `When to use`, then `What to produce`,
   then `Procedure`, then `Constraints`.
2. List the skill in the table above and in
   [`../AGENTS.md`](../AGENTS.md#5-skill-catalog-canonical-tool-independent).
3. (Optional) Add a VS Code Copilot wrapper under `../.github/agents/` so the
   skill is invocable from Copilot Chat.
4. Open a PR following the contribution rules.
