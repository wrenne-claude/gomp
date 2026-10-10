<img src="resources/brand/gomp.svg" alt="" width="96">

# gomp

A native Qt 6 / QML desktop app for the [omp](https://github.com/can1357/oh-my-pi) coding agent:
streaming conversation, tool calls with inline diffs and approvals, plan review, subagent lanes,
a session browser with branch timelines, and every omp setting behind a diff-reviewed editor.

# DISCLAIMER: THIS IS TOTAL AND UTTER VIBE CODED SLOP
I'm making this project as a proof of concept, with Opus 5.5 and GLM 5.3 Flash being the lead "developers", **not me.**

## Build

gomp is developed and tested on Linux (Fedora, KDE Plasma on Wayland). macOS and Windows are
supported by the build files but have not been tested yet; the sections below say what to expect.
You need Qt 6.6 or newer (Quick, QuickControls2, ShaderTools, Svg, Concurrent, Test), CMake 3.24+,
Ninja and a C++20 compiler. Qt DBus is optional.

gomp drives the [omp](https://github.com/can1357/oh-my-pi) binary, which must be installed
separately. `--mock` runs a scripted demo without it.

### Linux

`secret-tool` (libsecret) provides keyring storage. Fedora:

    sudo dnf install qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qtshadertools-devel qt6-qtsvg-devel libsecret
    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
    cmake --build build
    ./build/gomp                 # real omp (found on PATH or ~/.local/bin)
    ./build/gomp --mock          # scripted demo backend, writes nothing
    cmake --install build        # binary, .desktop file, icon, metainfo

### macOS

Install the Xcode command line tools (`xcode-select --install`) and [Homebrew](https://brew.sh), then:

    brew install cmake ninja qt
    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
    cmake --build build
    ./build/gomp                 # or ./build/gomp --mock

`brew install qt` must give you Qt 6.6 or newer (check with `brew info qt`). The result is a plain
executable, not an `.app` bundle; bundling with `macdeployqt` is not set up. omp is found on PATH
and in `~/.local/bin`, `~/.bun/bin`, `~/.npm-global/bin` and `/opt/homebrew/bin`.

### Windows

Install Visual Studio 2022 (the "Desktop development with C++" workload, which includes CMake and
Ninja) and Qt 6.6 or newer for MSVC 2022 64-bit from the [Qt installer](https://www.qt.io/download-qt-installer-oss).
In the installer, select the MSVC 2022 64-bit kit and Qt Shader Tools. Then, from the
"x64 Native Tools Command Prompt for VS 2022":

    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:\Qt\6.8.0\msvc2022_64 -DGOMP_BUILD_TESTS=OFF
    cmake --build build
    C:\Qt\6.8.0\msvc2022_64\bin\windeployqt.exe build\gomp.exe
    build\gomp.exe              # or build\gomp.exe --mock

Adjust the Qt path to your version. `windeployqt` copies the Qt DLLs next to the executable so it
runs outside the Qt prompt. omp is found on PATH and in `%USERPROFILE%\.bun\bin`,
`%USERPROFILE%\.local\bin` and `%APPDATA%\npm`.

### What differs outside Linux

| | Linux | macOS and Windows |
|---|---|---|
| API keys | stored in the OS keyring through `secret-tool` | not stored by gomp; use omp's own login, or provider environment variables (for example `ANTHROPIC_API_KEY`), which Settings > Providers lists |
| Desktop notifications | freedesktop D-Bus | in-app toasts only |
| Packaging | `.desktop`, metainfo, Flatpak manifest | none |
| Screenshot tour (`tools/shots.sh`) | works on a virtual KWin | Linux only |
| Unit tests | all run | the config-store test needs a POSIX shell and is skipped on Windows |

If the build fails on macOS or Windows, please open an issue with the compiler output.

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
