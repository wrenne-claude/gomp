# Architecture

Three layers; QML never sees raw omp output.

1. **Adapter** (`src/adapter`): `AgentBackend` interface emitting typed `AgentEvent`s
   (`AgentEvents.h`). `RpcBackend` speaks `omp --mode rpc-ui` (JSON lines, protocol v2 chunk
   reassembly, request correlation, timeouts, per-command reply unwrap). `AcpBackend` speaks
   `omp acp` for plan mode (RPC has no plan proposals). `OmpBackend` hands one session between them,
   stopping one process before the other opens the session file. `MockBackend` plays
   `resources/mock/*.json` scenarios through the real RPC parser. `OmpCli` runs one-shot
   subcommands; `OmpLocator` finds and version-checks omp.
2. **State** (`src/model`, `src/config`, `src/app`): `Workspace` (one omp process per project tab)
   owns `ConversationModel`, `TodoModel`, `SubagentModel`, `CommandModel` and derives the activity
   state driving the Corona. `WorkspaceManager`, `SessionIndex` (scans session files on a worker
   thread), `SessionTreeModel`, `ModelCatalog`, `SettingsModel` over `ConfigStore` (staged edits,
   diff preview, backups, global writes via `omp config set`, project writes via `YamlLite`),
   `ProviderStore` (keyring via `secret-tool`, `!secret-tool lookup` apiKeys), `McpStore`,
   `PaletteModel`, `ShortcutModel`. `AppController` is the `App` QML singleton that wires them.
3. **UI** (`qml/`): `Theme` and `Ui` singletons, `Controls/` (Templates-based library), `Shell/`,
   `Overlay/`, and one folder per screen. Every file uses `pragma ComponentBehavior: Bound`.

Streaming: deltas are applied immediately but views are notified at most every 33 ms; markdown is
split into prose/code in C++ and prose rendered to themed rich text (`md::toHtml`, cached).

## Adding a settings panel

1. Create `qml/Settings/MyPage.qml`. For lists of omp settings, use `GenericPage` and set ids in
   `configure()`: `App.settings.ids = App.settings.idsWithPrefix(["myprefix."])`. Put custom UI in
   `headerComponent`. Expose `title` and `subtitle` properties.
2. Add `{ id: "mypage", label, icon }` to `curated` in `SettingsScreen.qml` and a branch in the
   `pageLoader.source` expression.
3. Write values only through `App.settings.setValue(id, value)` so they stage, validate, show in the
   review diff and get backed up. Add a step to `qml/Capture/ScreenshotDriver.qml` and review it.
