# omp integration research

Researched against **omp 18.8.7** (`~/.local/bin/omp`, a Bun-compiled single binary, upstream
`can1357/oh-my-pi`, a fork of Mario Zechner's `pi` coding agent). Everything below was verified
by running the binary, reading `omp --help` / subcommand help, driving `--mode rpc-ui` from a
probe script, and reading the protocol definitions embedded in the binary
(`packages/coding-agent/src/modes/rpc/wire/*.ts`).

## 1. Integration surfaces found

| Surface | How | Verdict |
|---|---|---|
| **RPC** `omp --mode rpc` / `--mode rpc-ui` | JSON lines over stdio. Typed command catalog (`{type, id, ...params}` → `{type:"response", id, command, success, data|error}`), typed event stream, host UI requests. Protocol v1 by default, v2 negotiable (`negotiate_protocol`). | **Primary transport.** Richest surface: streaming events, subagents, session tree, models, login, compaction, stats, queue. |
| **ACP** `omp acp` | Agent Client Protocol (JSON-RPC 2.0 over stdio): `session/new|load|resume|fork|list|close|prompt|cancel|set_mode|set_config_option`, `session/request_permission`, `session/update` (`agent_message_chunk`, `agent_thought_chunk`, `tool_call(_update)`, `plan`, `current_mode_update`, `config_option_update`, `usage_update`, …) plus `_omp/usage`, `_omp/projects/list`, `_omp/sessions/listAll`, `_omp/extensions(/toggle)`. | **Plan mode only** (see §3). No subagent streaming, no tree, no login. |
| SDK | Generated TS/Python/Go/Rust RPC clients exist upstream. None for C++. | Not used; gomp implements the wire protocol directly in C++. |
| CLI subcommands | `omp config list|get|set|reset --json`, `omp models --json`, `omp plugin … --json`, `omp skill list --json`, `omp agents`, `omp login`, `omp stats`, `omp --export`. | Used for things outside a session (settings, model catalog, plugins). |
| Files | `~/.omp/agent/{config.yml, models.yml, mcp.json, keybindings.yml, sessions/**/*.jsonl, RULES.md}`; per project `.omp/`. | Read directly for fast listing (sessions); written only where no CLI exists. |
| PTY scraping | | **Not needed.** |

### RPC details that shape the adapter

* First frame is `{"type":"ready","protocolVersion":1,"supportedProtocolVersions":[1,2],"maxFrameBytes":1048576,...}`.
* `prompt` is acknowledged by a `response` (`PromptAck{agentInvoked}`) and later completed by a
  `prompt_result` event carrying the same `id` (`status: completed|aborted|error`, `sessionSettled`).
  Slash commands sent as prompts run locally (`agentInvoked:false`, output in `command_output`).
* Session events: `agent_start/end`, `turn_start/end`, `message_start/update/end`
  (`assistantMessageEvent` = `text_start/delta/end`, `thinking_*`, `toolcall_*`, `done`, `error`),
  `tool_execution_start/update/end`, `tool_stream_update`, `auto_compaction_start/end`,
  `auto_retry_start/end`, `model_changed`, `thinking_level_changed`, `notice`, `queue_update`,
  `goal_updated`, `todo_reminder`, `session_info_update`, `config_update`, `available_commands_update`,
  `session_settled`, `extension_error`, `rpc_frame_error`.
* `set_event_filter{messageUpdates:"delta"}` drops the accumulated snapshot from every
  `message_update`. gomp enables it to keep streaming cheap: it accumulates deltas itself.
* Subagents: `set_subagent_subscription{level:"events"}` → `subagent_lifecycle`,
  `subagent_progress`, `subagent_event` (the child's own session events). `get_subagents`,
  `get_subagent_messages` (incremental by byte offset), `cancel_subagent`, `steer_subagent`.
* **Tool approval** only works in `rpc-ui` mode (`hasUI`). omp then sends
  `extension_ui_request{method:"select", title:"<tool + args summary>", options:["Approve","Deny"]}`
  and blocks the tool until gomp answers `extension_ui_response{id, value}`. In plain `rpc` mode
  approval-requiring tools fail. **gomp always runs `--mode rpc-ui`.**
* Other host UI requests: `select`, `confirm`, `input`, `editor`, `ask` (multi-question, opt-in
  with `set_ask_dialog`), `cancel`, `notify`, `setStatus`, `setWidget`, `setTitle`,
  `set_editor_text`, `open_url` (OAuth login flow). All are rendered by gomp's modal system.
* State snapshot `get_state` → `SessionState` (model incl. `contextWindow`/`cost`, thinking level,
  streaming/compacting flags, queue, todo phases, context usage, goal, fast/slow mode, tok/s).
* Stats `get_session_stats` → tokens (input/output/reasoning/cache) and `cost`.

## 2. Capability catalog and how gomp surfaces each

| Capability | omp source | gomp surface | Mechanism |
|---|---|---|---|
| Chat, streaming, thinking, tool calls | RPC events | Agent view | RPC |
| Interrupt / steer / follow-up queue | `abort`, `steer`, `follow_up`, `queue_update`, `remove_queued_message` | Composer + queue chips | RPC |
| Tool approval | `extension_ui_request select` | Inline approval card + desktop notification | RPC (rpc-ui) |
| Model selection (session) | `get_available_models`, `set_model`, `cycle_model` | Status bar model picker | RPC |
| Thinking level | `get_available_thinking_levels`, `set_thinking_level` | Effort segmented control | RPC |
| Model roles (default, smol, slow, plan, task, vision, commit, tiny, memory, advisor, judge, web) | `config.yml: modelRoles` | Settings › Models | `omp config set modelRoles …` |
| Model catalog (provider, context window, pricing, modalities) | `omp models --json` | Model picker cards | CLI |
| Providers & API keys | env vars, `models.yml providers.<name>.apiKey`, `agent.db` (OAuth via `/login`) | Settings › Providers | Keyring + `apiKey: "!secret-tool lookup …"` (see §4) |
| OAuth login / logout | RPC `get_login_providers`, `login` (→ `open_url` + `input`), `get_logout_accounts`, `logout` | Settings › Providers | RPC |
| Modes: default, plan, fast, slow, goal, (prewalk, advisor, plan-yolo) | see §3 | Mode switcher | RPC / ACP / launch flags |
| Plan review | ACP `session/request_permission` with options "Approve and execute" / "Approve and compact context" / "Refine plan"; plan file `local://PLAN.md` | Plan panel | ACP |
| Todos / plan progress | `SessionState.todoPhases`, `set_todos`, ACP `plan` updates | Plan panel checklist | RPC (+ACP) |
| Sessions list | `~/.omp/agent/sessions/<cwd-slug>/<ts>_<id>.jsonl` (`title`, `session` header, entries with `id/parentId`) | Session browser | Direct read (header + light scan) |
| Resume / switch | `open_session`, `switch_session` | Session browser | RPC |
| Fork / branch | `fork{entryId?}`, `branch{entryId}`, `get_branch_messages` | Session browser, message context menu | RPC |
| Session tree | `get_tree` (entries with `id/parentId`, `leafId`) | Tree/timeline view | RPC (or file parse) |
| Rename | `set_session_name` | Session browser | RPC |
| Delete | (TUI `/session delete`) | Session browser | Move file to trash (`QFile::moveToTrash`) |
| Export | `export_html{outputPath}`, `omp --export <file>` | Session browser | RPC / CLI |
| Compaction | `compact{customInstructions}` → `CompactionResult{summary,tokensBefore}`, auto-compaction events, `set_auto_compaction` | Compaction sheet with before/after | RPC |
| Handoff | `handoff` | Command palette | RPC |
| Subagents | §1 | Subagent manager | RPC |
| Slash commands | `get_available_commands` / `available_commands_update` (name, aliases, description, input hint, subcommands, source) | Composer autocomplete + palette | RPC |
| Settings (532 keys, 11 tabs) | binary schema + `omp config list --json` | Settings | `resources/schema/omp-settings.json` (extracted by `tools/extract-omp-schema.mjs`) + CLI |
| Tool approval policy | `tools.approvalMode` (always-ask/write/yolo), `tools.approval.<tool>` (allow/prompt/deny) | Settings › Permissions | CLI |
| MCP servers | `~/.omp/agent/mcp.json`, `.omp/mcp.json` (`mcpServers.<name>{type,url|command,args,env}`) | Settings › MCP | Direct JSON edit (validated) |
| Plugins / extensions | `omp plugin list|enable|disable|install --json`, `disabledExtensions` | Settings › Extensions | CLI |
| Skills | `omp skill list --json`, `skills.*` settings | Settings › Skills | CLI |
| Themes | `theme.dark` / `theme.light` (TUI themes) | Settings › Appearance (omp TUI theme) | CLI. gomp's own theme is separate. |
| Keybindings | `~/.omp/agent/keybindings.yml` (`app.*`, `tui.*` ids) | Settings › Keybindings (gomp's own shortcuts; omp TUI bindings editable as a second table) | gomp settings / direct YAML |
| Usage / stats | `get_session_stats`, `_omp/usage`, `omp usage` | Status meter, usage popover | RPC |
| Workspace dirs | `--add-dir`, `/add-dir`, `/dirs` | Workspace sheet | launch flags / slash |
| Profiles | `--profile` | First-run / workspace options | launch flag |

## 3. Modes

* **Default** — normal agent.
* **Plan** — read-only exploration that writes `local://PLAN.md` then *proposes* it. RPC mode never
  installs a plan-proposal handler (only the TUI, ACP and `--plan-yolo` do), and `/plan` is not in the
  RPC command catalog. ACP exposes mode `plan` through `session/set_mode` and turns the proposal
  into `session/request_permission` (options *Approve and execute*, *Approve and compact
  context*, *Refine plan*).
  **Decision:** a gomp session switching into Plan mode hands its session file to an ACP transport
  (`session/load`, `session/set_mode plan`). After approval it hands the file back to RPC
  (`switch_session`). The adapter hides this; the UI just sees mode changes and a `PlanProposal`
  event. `--plan-yolo` (auto-approve) is offered as "Plan, then run".
  **Plan-mode questions and approvals (omp 18.8).** Over ACP the `ask` tool arrives as one
  `elicitation/create` form with fields `q0`, `q1`, … (a string with `oneOf`, or an array whose
  `items.anyOf` lists the options, for multi-select) and a free-text `q<N>__other` each; the answer is
  the form content with the chosen labels. gomp rebuilds it into the same question list RPC's `ask`
  request carries. ACP also wraps `bash`, `edit`, `delete` and `move` in `session/request_permission`
  unless `tools.approvalMode` is *explicitly* set to `yolo` (being yolo by default is not enough), and
  the request names the exact `toolCallId`. gomp answers those itself when the effective approval mode
  (and any per-tool `tools.approval` policy) would allow the call, so plan mode behaves as the
  setting shown in Settings says.
* **Fast** — `set_fast_mode`. **Slow** — `set_slow_mode` (when `slowModeSupported`).
* **Goal** — `goal{op:create|pause|resume|drop|get}`, `goal_updated`; plan and goal exclude each other.
* **Prewalk / Advisor** — launch flags `--prewalk`, `--advisor`; also settings.

## 4. Things omp does not expose programmatically, and gomp's handling

| Gap | Handling |
|---|---|
| Plan mode over RPC | ACP transport handoff (§3). |
| Writing settings with diff preview | gomp computes the diff on the **key/value view** (`omp config list --json` before vs. proposed), backs up `config.yml` to `~/.local/state/gomp/backups/`, then writes through `omp config set <key> <json>` so omp validates the value and keeps unknown keys. |
| Per-project overrides | `.omp/config.yml` in the project. omp's CLI writes global config only, so gomp writes project files directly using a lossless YAML layer (unknown keys kept, refuses anchors/flow maps and drops to the raw editor). |
| API keys in keyring | omp runs any `apiKey` that starts with `!` as a shell command (`resolve-config-value.ts`). gomp stores the secret with `secret-tool store --label="gomp: <provider>" service gomp provider <name>` (password over stdin, never argv), then sets `apiKey: "!secret-tool lookup service gomp provider <name>"` in `models.yml`. Built-in providers that read env vars get their key injected into the omp process environment at spawn. Plaintext keys already in `models.yml` are flagged in Settings with a one-click "Move to keyring". On KDE Plasma 6, KWallet provides the Secret Service API, so the same path covers both. |
| Session delete | No RPC/CLI. gomp moves the JSONL to the trash (reversible). |
| Session list across projects | RPC lists only the current directory. gomp scans `sessions/*/` itself (first lines only), which is also much faster than spawning omp. |
| Tool diffs accept/reject | omp approves *calls*, not hunks. gomp shows the edit/write diff inside the approval card; "Accept" = Approve, "Reject" = Deny. There is no per-hunk accept. |
| omp TUI theme preview | Not renderable; gomp lists names and writes the setting. |

## 5. Version handling

gomp runs `omp --version` (`omp/X.Y.Z`) on start. The adapter is tested against 18.8.x.
Older majors get a warning banner. Missing commands come back as `success:false`, which gomp surfaces
per action instead of crashing. If `omp` is not on `PATH` or at the configured path, the first-run
screen offers locate/install (install = the upstream installer command, shown for copying, never run
silently). A crashed omp process marks the workspace *Disconnected* and offers Restart, which
reopens the same session file.
