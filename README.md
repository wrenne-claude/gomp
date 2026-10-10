# gomp

A native Qt 6 / QML desktop app for the [omp](https://github.com/can1357/oh-my-pi) coding agent:
streaming conversation, tool calls with inline diffs and approvals, plan review, subagent lanes,
a session browser with branch timelines, and every omp setting behind a diff-reviewed editor.

## Build

Requires Qt 6.6+ (Quick, QuickControls2, ShaderTools, Svg, DBus, Concurrent, Test), CMake 3.24+, a
C++20 compiler, and `secret-tool` (libsecret) for keyring storage. Fedora:

    sudo dnf install qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qtshadertools-devel qt6-qtsvg-devel libsecret
    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
    cmake --build build
    ./build/gomp                 # real omp (found on PATH or ~/.local/bin)
    ./build/gomp --mock          # scripted demo backend, writes nothing
    cmake --install build        # binary, .desktop file, icon, metainfo

Options: `--project <dir>`, `--size 2560x720`, `--theme dark|light`, `--reduce-effects`,
`--screenshots <dir>` (renders every state and exits; see `tools/shots.sh`).

## Test

    cd build && ctest                                  # unit, parser, config and mock end-to-end tests
    GOMP_TEST_REAL_OMP=1 ./tests/tst_realomp           # live omp handshake + plan-mode handoff, no prompts
    cmake --build build --target gomp_qmllint          # QML lint (clean)

## Docs

- `docs/omp-integration.md`: how gomp talks to omp (RPC + ACP), capability catalog, gaps
- `docs/design.md`: tokens, components, motion, the Corona, screen inventory
- `docs/architecture.md`: layers, data flow, adding a settings panel
