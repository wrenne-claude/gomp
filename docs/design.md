# gomp design brief

gomp is a flight deck for one or more coding agents. A developer glances at it to see what the
agent is doing right now (thinking, writing, running a tool, waiting on them), steers it, and
configures it. The emotional target is a well-made avionics glass cockpit at night: dark, calm,
precise, with light used only to report state.

## Concept

**Sodium and blueprint.** The default accent is *sodium*, the warm amber of cockpit
annunciators and night-time instrument lighting. It is the colour of "the agent is doing
something for you". Plan mode switches the ambient light to *blueprint* (cool cartographic blue).
Goal mode uses *lock* (signal green). Each mode keeps the same surfaces and changes only the light.

Things I deliberately avoided, and why:
* the purple-to-blue AI gradient, and cyan-on-black "hacker" HUDs: they read as generic AI tooling
* neutral #0B0B0B blacks: the base is a blue-graphite *hull* colour, so frosted panels have
  something to tint
* glass on everything: only floating layers (palette, popovers, modals, inspector over content) are
  frosted. Docked panels are opaque.

One memorable element, everything else quiet: the **Corona** (the activity visualisation, below).

## Information architecture

```
┌ Titlebar: logo │ workspace tabs …………………… Corona │ ⌘K │ — □ × ┐
├──────┬──────────────────────────────────────────┬────────────┤
│ Rail │ Screen (Agent | Sessions | Subagents |   │ Inspector  │
│      │         Settings)                        │ (Plan,     │
│      │                                          │  Context,  │
│      │                                          │  Agents)   │
├──────┴──────────────────────────────────────────┴────────────┤
│ Status strip: mode │ model │ effort │ context meter │ cost │ branch │ cwd │
└───────────────────────────────────────────────────────────────┘
```

* **Rail** (56px): Agent, Sessions, Subagents, Settings, then the shortcut sheet. Workspaces live
  as titlebar tabs, not in the rail, so a 720px-tall screen keeps its vertical space.
* **Workspace** = one omp process, one cwd, one live session. The tab shows name, mode light and a
  miniature Corona dot.
* **Inspector** is contextual to the Agent screen. It docks when width ≥ 1360px and becomes a
  frosted overlay drawer below that.

### Responsive rules
| Condition | Behaviour |
|---|---|
| width < 900 | rail collapses to icons only (it already is), inspector is overlay, status strip hides cwd |
| width ≥ 1360 | inspector docked (360px) |
| width ≥ 2000 | conversation column capped at 920px and centred; inspector widens to 440px; Subagents shows lanes side by side |
| height < 820 ("short") | `Theme.compact`: tighter vertical rhythm, composer max 6 lines, titlebar 40px. The 28px status strip stays; merging it into the composer was dropped because the readouts need a fixed place. |

The ultrawide target (2560×720) is "wide and short": compact density, docked inspector, centred
conversation, and Subagents lanes laid out horizontally.

## Tokens (single source: `qml/Theme/Theme.qml`; shell state in `qml/Theme/Ui.qml`)

### Colour (dark is the hero)
| Token | Dark | Light | Use |
|---|---|---|---|
| `hull` | #0D1117 | #EEF1F5 | window background |
| `surface0` | #121821 | #F7F9FB | docked panels |
| `surface1` | #18202B | #FFFFFF | cards, inputs |
| `surface2` | #202A37 | #F1F4F8 | hover / raised |
| `hairline` | #263142 | #D5DCE5 | 1px borders |
| `hairlineStrong` | #33415A | #B9C3D0 | focusable control borders |
| `textPrimary` | #E7ECF3 | #121821 | body |
| `textSecondary` | #A3AFBF | #4A5668 | secondary |
| `textTertiary` | #74829A | #6B778A | hints, meta (AA on surface0 for ≥ 14px) |
| `sodium` (accent default) | #FFB547 | #B86E00 | primary actions, agent activity |
| `blueprint` | #6CB4FF | #1F6FD1 | plan mode |
| `lock` | #5BD49B | #1E8A57 | goal mode, success |
| `danger` | #FF6B6B | #C62E2E | errors, deny, destructive |
| `caution` | #F2D35B | #8A6D00 | warnings (always paired with the caution glyph so it never relies on hue next to sodium) |
| `info` | #8FA7FF | #3A55C9 | info notices |

The accent is user-selectable (Sodium, Blueprint, Lock, Ion #4FD1C5, Rose #F27BA6, System = KDE
accent). Mode light overrides the *ambient* hue only, never the accent of controls.
Text on the accent uses `onAccent` (#1A1206 dark / #FFFFFF light). Contrast pairs are checked in
`tests/tst_contrast.cpp`.

### Type
* **UI:** Barlow (OFL), by Jeremy Tribby, based on California highway signage: low-contrast,
  slightly rounded, legible at speed. It fits instruments without being a costume.
* **Readouts:** Barlow Semi Condensed with tabular figures (`tnum`) for meters, costs, token counts
  and durations, so digits never jitter while they animate.
* **Code:** JetBrains Mono (OFL).

| Role | Family | Size/Line | Weight |
|---|---|---|---|
| `display` | Barlow | 28/34 | 600 |
| `title` | Barlow | 20/26 | 600 |
| `heading` | Barlow | 16/22 | 600 |
| `body` | Barlow | 14/19.6 (1.4) | 400 |
| `bodyStrong` | Barlow | 14/19.6 (1.4) | 500 |
| `caption` | Barlow | 12/16 | 500 |
| `readout` | Barlow Semi Condensed | 13/16 | 500, tnum |
| `readoutLarge` | Barlow Semi Condensed | 22/26 | 500, tnum |
| `code` | JetBrains Mono | 13/20 | 400 |

Prose is capped at 76ch (`Theme.proseMaxWidth`). No all-caps labels, no tracked-out eyebrows.

Ligatures are off everywhere (`liga`, and `calt` for code): Barlow joins `--` into a dash and
JetBrains Mono joins `===`, both of which misrepresent flags and code. Body line height applies only
to text that wraps; single-line labels use 1.0 so they stay optically centred in chips and rows.

### Space, size, radius
* Spacing on a 4px base: `s1`=4, `s2`=8, `s3`=12, `s4`=16, `s5`=24, `s6`=32, `s7`=48, `s8`=64.
* Control heights: `controlSm`=28, `controlMd`=32, `controlLg`=40.
* Radii carry hierarchy: `rSm`=4 (chips, kbd), `rMd`=8 (controls, rows), `rLg`=12 (cards),
  `rXl`=16 (sheets, palette). Pills only for status lights.

### Elevation
| Level | Use | Treatment |
|---|---|---|
| 0 | hull, docked panels | flat, hairline separation |
| 1 | cards, inputs | `surface1` + hairline + 1px inner top highlight (white 4%) |
| 2 | popovers, dropdowns, tooltips | frosted (blur 32, tint `surface1` @ 82%), shadow 0 8 24 @ 40% |
| 3 | modals, palette, drawers | frosted (blur 48), shadow 0 16 48 @ 50%, scrim `hull` @ 55% |

Elevation means "floats above and can be dismissed". Docked things never get shadows.

### Motion
| Token | Value | Use |
|---|---|---|
| `fast` | 120ms | hover, press, focus ring |
| `base` | 180ms | toggles, tab indicator, list row expand |
| `slow` | 260ms | sheets, drawers, shared-element morphs |
| `meter` | 400ms | number/meter tweening (feels instrument-like) |
| `easeOut` | `Easing.OutCubic` | entering |
| `easeIn` | `Easing.InCubic` | leaving (duration × 0.75) |
| `spring` | SpringAnimation spring 4.5, damping 0.32 | indicators, drawers, palette |

Rules: motion answers an action or reports state. Only list *first appearance* staggers (24ms per
item, max 8 items). With *Reduce motion*, all durations collapse to 0 except opacity crossfades
(80ms) and the Corona drops to a static glyph whose colour still reports state.

## Corona: the activity visualisation

A circular field rendered by one fragment shader (`resources/shaders/corona.frag`), driven by
real adapter state:

| Agent state | Visual | Data bound |
|---|---|---|
| idle | thin ring, slow breathing (6s period), 30% intensity | — |
| thinking | inward-drifting interference bands (polar noise flowing to the centre) | thinking delta rate |
| streaming | outward pulses whose frequency follows the measured tokens/s | `tokensPerSecond` |
| tool running | ring breaks into arc segments that rotate; one segment per running tool | active tool count |
| subagents | small satellites orbit the ring, one per running subagent | subagent count |
| awaiting approval | ring stops; a single sodium beacon pulses at 1Hz on the rim | pending approvals |
| error | brief radial shudder, then danger tint for 2s | last error |

It appears at three sizes: 20px in the titlebar (always visible), 12px in workspace tabs, and
large on a fresh session's empty conversation. Ambient light is a very low-alpha radial wash behind the
conversation in the mode hue, using the same intensity curve. With *Reduce effects* the shader is
swapped for a static SVG ring tinted by state.

Under the transcript, the working line says "Working…" with omp's own shimmer (`shimmer.frag`): a
cosine band of the mode colour sweeps over dim text at 30 characters a second, as omp's terminal
does. Reduced motion or effects leaves the label static in the mode colour.

## Component inventory (`qml/Controls`, all built on QtQuick.Templates)
Button (primary, secondary, ghost, danger; sm/md/lg; loading), IconButton, Toggle, Checkbox,
Slider, SegmentedControl, TextField, SearchField, TextArea, Dropdown (popover list with search when
> 8 items), ListRow, Card, Tabs, Tooltip, Popover, Badge, StatusLight, Kbd, Meter, Skeleton,
EmptyState, ErrorState, Surface (elevation levels), Divider, ScrollBar, FocusRing, Icon.
Overlays: `GSheet` (modal: focus trapped by the modal popup, Esc closes, scrim, scrolling body),
`GPopover`, `GMenu`, `GToolTip`, all on `GFloating` (frosted backdrop sample + analytic shadow);
`Overlay/ToastHost` (max 3, 4 s, errors 8 s, pause on hover), `Overlay/CommandPalette`,
`Overlay/ShortcutSheet`, `Overlay/UiRequestHost` (omp's select/confirm/input/editor/ask/open_url).
Templates popups have no implicit size: every popup control sets `implicitWidth/Height` from its
content, or it renders at zero height.

Every interactive control has hover, pressed, focus (2px accent ring offset 2px), disabled (40%
content opacity, no hover) and, where applicable, loading states. Each sets `Accessible.role` and
`Accessible.name`.

## Iconography
One 20px grid SVG set with a 1.5px stroke, round caps and joins (`resources/icons`, 85 icons),
recoloured by an image provider (`image://icons/<name>/<color>`) that rasterises at device pixel
ratio, so there is no per-icon shader. No emoji, no mixed packs. The gomp logo (`resources/brand/gomp.png`, with a pixel-exact SVG
for the desktop icon) is 10x10 pixel art. It appears as the window icon, at the left of the
titlebar, and on the first-run and "open a project" screens. `GLogo` scales it with nearest-neighbour
sampling at multiples of 10px so the pixels stay square. The Corona stays a status display, never a
logo.

## Screen inventory
1. First run: locate omp, version check, install instructions, pick first project folder.
2. Agent: conversation (user, assistant markdown, reasoning, tool cards, diffs, approvals, notices),
   composer (slash autocomplete, @file picker, image attach, stop), status strip, mode switcher,
   inspector (Plan, Context, Agents tabs).
3. Plan panel: plan document, editable checklist, approve / approve and compact / refine / reject,
   live step progress.
4. Sessions: search, sort, project filter, list with previews; detail pane with actions and the
   branch tree; compaction sheet.
5. Subagents: lane view of the delegation hierarchy; detail drawer with streamed output, cancel
   and steer.
6. Settings: left nav (Providers, Models, Modes, Permissions, Tools, MCP, Extensions and skills,
   Appearance, Keybindings, all omp tabs, Advanced), global search, scope switch (Global / Project).
7. Overlays: command palette, shortcut sheet, diff preview before config writes, confirm, toasts.

## Copy rules
Sentence case. Buttons say what happens ("Approve and run", "Move key to keyring"). Errors say what
broke and how to fix it ("omp exited with code 1. Restart the workspace, or open the log."). No
exclamation marks, no "Oops".

## Visual review

`tools/shots.sh [filter]` renders every screen and state (dark, light, 2560x720, 980x760,
reduced effects) into `screenshots/` on a private virtual KWin, using the mock backend. Review the
images against this document before calling a screen done.
