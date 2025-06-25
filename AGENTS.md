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

1. Ensure SDL2, SDL2_mixer, SDL2_ttf, SDL2_net and SDL2_image development headers are installed.
2. From the repository root, run one of:

```bash
cd src
make -f makefile.sdl2 tomenet       # Linux SDL2 build
make -f makefile.sdl2 tomenet.exe   # Windows SDL2 build (cross-compile)
make -f makefile.sdl2 all           # build both binaries
```
The resulting binaries (`tomenet` and/or `tomenet.exe`) will appear in `src/`.

**Running**

Copy the binary from `src/` to the repository root (or run `make install`) before run,
so that resources load correctly.

SDL2 builds load `tomenet.cfg` from the SDL2 preferences directory obtained via
`SDL_GetPrefPath(SDL2_ORG_NAME, SDL2_GAME_NAME)`. On Linux this typically
resolves to `~/.local/share/TomenetGame/tomenet/`. The `tomenet.cfg` will be copied there
automatically, but you may copy and edit `tomenet.cfg` there if needed for debugging.

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
Use the following command to verify that both the Linux and Windows SDL2 clients compile:

```bash
cd src
make -f makefile.sdl2 all
```

