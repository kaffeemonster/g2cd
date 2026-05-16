<!-- gitnexus:start -->
# GitNexus — Code Intelligence

This project is indexed by GitNexus as **g2cd** (5884 symbols, 12470 relationships, 300 execution flows). Use the GitNexus MCP tools to understand code, assess impact, and navigate safely.

> If any GitNexus tool warns the index is stale, run `npx gitnexus analyze --skip-agents-md` in terminal first.

## Always Do

- **MUST run impact analysis before editing any symbol.** Before modifying a function, class, or method, run `gitnexus_impact({target: "symbolName", direction: "upstream"})` and report the blast radius (direct callers, affected processes, risk level) to the user.
- **MUST run `gitnexus_detect_changes()` before committing** to verify your changes only affect expected symbols and execution flows.
- **MUST warn the user** if impact analysis returns HIGH or CRITICAL risk before proceeding with edits.
- When exploring unfamiliar code, use `gitnexus_query({query: "concept"})` to find execution flows instead of grepping. It returns process-grouped results ranked by relevance.
- When you need full context on a specific symbol — callers, callees, which execution flows it participates in — use `gitnexus_context({name: "symbolName"})`.

## Never Do

- NEVER edit a function, class, or method without first running `gitnexus_impact` on it.
- NEVER ignore HIGH or CRITICAL risk warnings from impact analysis.
- NEVER rename symbols with find-and-replace — use `gitnexus_rename` which understands the call graph.
- NEVER commit changes without running `gitnexus_detect_changes()` to check affected scope.

## Resources

| Resource | Use for |
|----------|---------|
| `gitnexus://repo/g2cd/context` | Codebase overview, check index freshness |
| `gitnexus://repo/g2cd/clusters` | All functional areas |
| `gitnexus://repo/g2cd/processes` | All execution flows |
| `gitnexus://repo/g2cd/process/{name}` | Step-by-step execution trace |

## CLI

| Task | Read this skill file |
|------|---------------------|
| Understand architecture / "How does X work?" | `.agents/skills/gitnexus/gitnexus-exploring/SKILL.md` |
| Blast radius / "What breaks if I change X?" | `.agents/skills/gitnexus/gitnexus-impact-analysis/SKILL.md` |
| Trace bugs / "Why is X failing?" | `.agents/skills/gitnexus/gitnexus-debugging/SKILL.md` |
| Rename / extract / split / refactor | `.agents/skills/gitnexus/gitnexus-refactoring/SKILL.md` |
| Tools, resources, schema reference | `.agents/skills/gitnexus/gitnexus-guide/SKILL.md` |
| Index, status, clean, wiki CLI commands | `.agents/skills/gitnexus/gitnexus-cli/SKILL.md` |

<!-- gitnexus:end -->

# CRITICAL RULES - MUST FOLLOW

## RESPNSES

- keep responses concise and to the point - unless the user asks otherwise

## PLANING MODE

- Always ask clarifying questions
- Bever asume design, tech stack or features
- Use deep-dive sub-agents to assist with research
- Use deep-dive sub-agents to review the different aspects of your plan before presenting it zo the user

## CHANGE / EDIT MODE

- never implement features youself when possible - use sub-agents
- Identify changes from the plan that can be implemented in parallel, and use sub-agents to implement the features efficiently
- When using sub-agents to implement features, act as a coordinator only
- Use the best model for the task - premium models for complex task (like coding, reasening) and mid-tier models for simpler tasks

## TESTING

- Use any testing tools, libraries available to the project for testing your changes
- Never assume your changes simply work, always test!
- If Project does not have any testing tools, scripts, MCP tools, skills, etc. available for testing, ask the user wheter testing should be skipped.

# Build and Development
-- **Build Process:** `./configure` $\rightarrow$ `make`.
-- **Release Build:** `./configure --enable-release` $\rightarrow$ `make`.
-- **Install:** `make install`.
-- **Run:** `./g2cd`.
-- **Developer Tools:**
-- `make todo`: Generate TODO list from source.
-- `make callgraph`: Generate Graphviz callgraph.
-- **Clean:** `make clean` or `make distclean`.
--** **Distribute:** `make dist`

# Coding style
- real tabs for indent, visual align with spaces
- braces on new line, except for very short blocks (3 to 4 lines)
- snake_case, no camelCase
- /\* \*/ comments, except TODO:, put in first column to put in `make todo`

# Performance Constraint: C10k with Heavy State
- Use `.agents/references/PERFORMANCE.md` for detailed guidiance
- **Scalability:** Solutions must not scale state linearly or worse without resource consideration.
- **Metric:** Performance must remain stable under 10k connections.

# Security Constraint:
- **Network Facing:** Assume all incoming data is hostile.
- Use `.agents/references/SECURITY.md` for detailed guidiance

# Hazard Pointer
- **DO NOT MODIFY:** `hzp_scan()` function in `lib/hzp.c` is highly optimized lock-free logic.
- **Usage:** Agents should **only** use the `hzp_ref`/`hzp_unref`/`hzp_deferfree` API.
    - Use `lib/HZP_REFERENCE.md` for correct usage patterns.
    - Never access hazard pointer protected shared data without `hzp_ref()`/`hzp_unref()`.
