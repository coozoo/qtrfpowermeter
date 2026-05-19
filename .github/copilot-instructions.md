# Copilot instructions for `coozoo/qtrfpowermeter`

## Project at a glance
- Qt6/C++17 desktop application (`qmake` project) for serial-port RF power meters.
- Main project file: `rf8000.pro`.
- Entry point: `main.cpp`.
- Core application code: `src/`.
- Third-party bundled code: `3rdparty/` (`qcustomplot`, `spline`).
- Packaging/build metadata: `debian/`, `qtrfpowermeter.spec`, `.github/workflows/main.yml`.

## How to build and validate
- Primary local build flow (Linux):
  - `qmake6`
  - `make -j$(nproc)`
- Debian packaging flow:
  - `dpkg-buildpackage -us -uc`
- There is currently no dedicated unit-test suite or linter configuration in this repository; validation is build/package success.

## Environment and dependencies
- Requires Qt6 development tooling and modules (see CI and packaging manifests):
  - `qt6-base-dev`
  - `qt6-tools-dev`
  - `qt6-tools-dev-tools`
  - `qt6-l10n-tools`
  - `libqt6charts6-dev`
  - `libqt6serialport6-dev`
  - `libqt6svg6-dev`
  - `libgl1-mesa-dev`
  - `libssl-dev`

## CI behavior that affects agent work
- Workflow: `.github/workflows/main.yml` (`Release_Version`).
- Build tags in commit/PR text control platform builds:
  - `[build linux]`, `[build win]`, `[build mac dmg]`, `[build mac zip]`, `[build mac]`.
- `[skip ci]` intentionally skips workflow execution.
- If no build tags are present, CI defaults to building all platforms.

## Recommended editing approach
- Keep changes minimal and scoped; avoid broad refactors unless requested.
- For device/protocol behavior, inspect:
  - `src/rf8000device.*`
  - `src/rfpmv5device.*`
  - `src/rfpmv7device.*`
  - `src/serialportinterface.*`
- For UI behavior, inspect:
  - `src/mainwindow.*` and related `.ui` files.
- Preserve existing qmake/project structure and packaging files unless task explicitly targets build/release configuration.

## Errors encountered and workarounds
- Encountered during onboarding validation:
  - Running `qmake6 && make -j$(nproc)` failed with `qmake6: command not found`.
- Workaround:
  - Install Qt6 toolchain/dependencies first (see dependency list above, also reflected in `debian/control` and `.github/workflows/main.yml`), then re-run:
    - `qmake6`
    - `make -j$(nproc)`
