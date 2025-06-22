# AGENTS.md

**Project Overview**

This repository contains the source code of the TomeNET SDL2 client for Linux. The code base is in C and lives under the `src` directory. The repository also includes scripts and resources for running the game, but the focus here is the SDL2 client.

**Key directories**

- `src/` – C source tree.
  - `client/` – cross-platform client code (SDL2 entry `main-sdl2.c`, X11 in `main-x11.c`, Windows in `main-win.c`).
  - `common/` – utilities and networking shared between client and server.
  - `server/`, `world/`, `account/` – server and gameplay code not needed for the client build.
  - `console/` – console client.
  - `preproc/` – custom Lua preprocessor used during the build.
- `lib/` – runtime data: configs, fonts, graphics.
- Documentation: `README.md`, `TomeNET-Guide.txt` and PDF versions.
- Scripts such as `runserv`, `tomenet-direct-*.sh` help run the game.

**Building**

1. Ensure SDL2, SDL2_mixer, SDL2_ttf and SDL2_net development headers are installed.
2. From the repository root, run:

```bash
cd src
make -f makefile.sdl2 tomenet
```
The resulting `tomenet` binary will appear in `src/`.
**Running**

The executable needs to be run from repository root folder.
After building copy `tomenet` out from `src/` directory to the repository root or run `make install`.
Configuration is read from `~/.tomenetrc` (copy `tomenet.cfg` there if needed).
In headless containers SDL2 will fail to open a window, but you can still debug other code.
**Compilation macros**

The same client sources build multiple front-ends. Important macros include:
- `USE_SDL2` – SDL2 renderer for Linux and Windows (`makefile.sdl2`).
- `USE_X11` – X11 windowing (`makefile`, `makefile.osx`).
- `WINDOWS` – Windows build (`makefile.win`).
- `USE_GCU` – console client using ncurses (`makefile.gcu`).
Other legacy macros (e.g. Amiga) also exist. SDL2 code is intended to remain OS independent.
**Code style**

- Source files are written in C.
- Tabs are used for indentation.

**Testing**

There are currently no automated tests in this repository. Building the project is the main validation step.

