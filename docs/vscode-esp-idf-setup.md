# VS Code Setup for ESP-IDF Development on Fedora

This document describes how to set up VS Code with code intelligence (autocomplete, go-to-definition, hover docs) and build/flash/monitor integration for ESP-IDF projects on Fedora Linux.

Tested with:
- Fedora 43, Python 3.14
- ESP-IDF v6.1-dev (cloned from GitHub)
- VS Code Extension: Espressif IDF v2.0.2
- VS Code Extension: clangd (llvm-vs-code-extensions.vscode-clangd)
- Espressif clangd esp-20.1.1

## Overview

Two extensions work together:

1. **Espressif IDF extension** (`espressif.esp-idf-extension`) -- GUI for build, flash, monitor, and project setup.
2. **clangd extension** (`llvm-vs-code-extensions.vscode-clangd`) -- code intelligence (autocomplete, hover, go-to-definition). Uses Espressif's custom clangd binary that understands the RISC-V cross-compilation target.

The Microsoft C/C++ IntelliSense must be disabled to avoid conflicts with clangd. Add this to your global VS Code settings (`~/.config/Code/User/settings.json`):

```json
"C_Cpp.intelliSenseEngine": "disabled"
```

## Prerequisites

### ESP-IDF Installation

ESP-IDF cloned from GitHub (not installed via `eim` GUI, which has issues on Fedora with Python 3.14 and Ubuntu-specific library names):

```bash
mkdir -p ~/esp
cd ~/esp
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh
```

This installs toolchains and Python environments into `~/.espressif/`.

### Install esp-clang

The system clangd (from Fedora repos) does not understand Espressif's custom RISC-V CPU targets. You need Espressif's clangd:

```bash
source ~/esp/esp-idf/export.sh
python ~/esp/esp-idf/tools/idf_tools.py install esp-clang
```

This installs to `~/.espressif/tools/esp-clang/`. Note the exact version path -- you'll need it for VS Code settings.

Find the clangd binary path:

```bash
find ~/.espressif/tools/esp-clang -name "clangd" -type f
```

Example: `~/.espressif/tools/esp-clang/esp-20.1.1_20250829/esp-clang/bin/clangd`

### Build the Project Once

clangd needs `build/compile_commands.json` to understand the project. Build at least once:

```bash
source ~/esp/esp-idf/export.sh
cd ~/esp/your-project
idf.py build
```

## Configuration Files

### 1. EIM IDF JSON (`~/.espressif/tools/eim_idf.json`)

The Espressif IDF extension v2.0.x reads this file to find ESP-IDF installations. The old `idf.espIdfPath` and `idf.toolsPath` settings are removed in v2.0.

If this file is empty or missing, the extension shows "No ESP-IDF Setups found." Create or edit it:

```json
{
  "gitPath": "/usr/bin/git",
  "idfToolsPath": "/home/YOUR_USER/.espressif",
  "idfSelectedId": "esp-idf-v6.1",
  "idfInstalled": [
    {
      "id": "esp-idf-v6.1",
      "name": "ESP-IDF v6.1",
      "path": "/home/YOUR_USER/esp/esp-idf",
      "idfToolsPath": "/home/YOUR_USER/.espressif",
      "python": "/home/YOUR_USER/.espressif/python_env/idf6.1_py3.14_env/bin/python",
      "activationScript": "/home/YOUR_USER/esp/esp-idf/export.sh"
    }
  ],
  "eimPath": "/usr/bin/eim",
  "version": "1.0"
}
```

Adjust `YOUR_USER`, ESP-IDF version, and Python env path to match your system. Find the correct Python env:

```bash
ls ~/.espressif/python_env/
```

### 2. VS Code Workspace Settings (`.vscode/settings.json`)

```json
{
  "cmake.configureOnOpen": false,
  "clangd.path": "/home/YOUR_USER/.espressif/tools/esp-clang/esp-20.1.1_20250829/esp-clang/bin/clangd",
  "clangd.arguments": [
    "--compile-commands-dir=${workspaceFolder}/build",
    "--background-index",
    "--query-driver=/home/YOUR_USER/.espressif/tools/**/*-elf-*"
  ],
  "terminal.integrated.env.linux": {
    "IDF_PATH": "/home/YOUR_USER/esp/esp-idf"
  },
  "idf.port": "/dev/ttyACM0",
  "idf.flashType": "UART"
}
```

Key settings:
- `clangd.path` -- points to Espressif's clangd, not the system one
- `--query-driver` -- allows clangd to query the cross-compiler for system includes
- `--compile-commands-dir` -- tells clangd where to find the compilation database
- `idf.port` -- your ESP32 serial port (check with `ls /dev/ttyACM*` or `ls /dev/ttyUSB*`)

### 3. clangd Config (`.clangd`)

ESP-IDF's GCC toolchain uses flags that clangd doesn't understand. This file strips them:

```yaml
CompileFlags:
  Remove:
    - -fno-shrink-wrap
    - -fstrict-volatile-bitfields
    - -fno-tree-switch-conversion
    - -fno-jump-tables
    - -fzero-init-padding-bits=all
    - -fno-malloc-dce
    - -mtune=esp-base
    - -specs=picolibc.specs
    - -march=rv32imc_zicsr_zifencei
    - -freorder-blocks
  Add:
    - -march=rv32imc
```

The `Add` section provides a simplified `-march` that clangd understands as a replacement for the removed one.

**Target-specific notes:**
- ESP32-C3, C6, H2 (RISC-V): use `-march=rv32imc` as shown above
- ESP32, S2, S3 (Xtensa): remove the `-march` lines and adjust for the Xtensa architecture

## Troubleshooting

### "No ESP-IDF Setups found"

The extension reads `~/.espressif/tools/eim_idf.json`. Make sure `idfInstalled` array is populated (see section above). If you previously had a different ESP-IDF version installed, stale entries in this file or in `~/.espressif/esp_idf.json` can cause errors.

### "The path argument must be of type string. Received undefined"

Set `idf.port` in `.vscode/settings.json` to your serial port (e.g., `/dev/ttyACM0`).

### clangd shows "invalid AST" / "unknown target CPU"

Check the Output panel (select "clangd" from dropdown). Common causes:

1. **Using system clangd instead of Espressif's** -- verify `clangd.path` points to the esp-clang binary, not `/usr/bin/clangd`. The startup log shows `argv[0]`.
2. **Missing flags in `.clangd` Remove list** -- if you see `error: unknown target CPU 'esp-base'` or similar, add the offending flag to the Remove list.
3. **Missing `compile_commands.json`** -- run `idf.py build` at least once.

### "Couldn't build compiler instance" for ESP-IDF components

Background indexing errors for files under `esp-idf/components/` are normal. These components may use different compile flags that clangd can't process. Your project files should still work correctly -- check for "Built preamble" messages for your source files in the clangd output.

### Fedora-specific: `eim install` fails with missing prerequisites

`eim` checks for Debian package names. Fedora equivalents:

| eim expects (Debian) | Fedora package |
|---|---|
| `libffi-dev` | `libffi-devel` |
| `libusb-1.0-0` | `libusb1` |
| `libssl-dev` | `openssl-devel` |
| `libgcrypt20` | `libgcrypt` |
| `libglib2.0-0` | `glib2` |
| `libpixman-1-0` | `pixman` |
| `libsdl2-2.0-0` | `SDL2` |
| `libslirp0` | `libslirp` |

Install them, then use `eim install --skip-prerequisites-check` since eim will still complain about the Debian names.

## CMakeLists.txt Note

When you add `REQUIRES` or `PRIV_REQUIRES` to your component's `CMakeLists.txt`, ESP-IDF stops linking the default set of common components and only links what you explicitly list. This means you may need to add dependencies that previously worked via transitive includes (e.g., `esp_adc`, `esp_driver_gpio`).
