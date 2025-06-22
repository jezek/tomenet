# AGENTS.md

**Project Overview**

This repository contains the source code of the TomeNET SDL2 client for Linux. The code base is in C and lives under the `src` directory. The repository also includes scripts and resources for running the game, but the focus here is the SDL2 client.

**Key directories**

- `src/client/` – main client implementation. SDL2 specific code is in `main-sdl2.c`.
- `src/common/` – shared utilities used by both client and server.
- `src/server/`, `src/world/`, and related directories – server side code; usually not needed when working on the client.
- `lib/` – data files (fonts, graphics, configs) used at runtime.

**Building**

1. Ensure SDL2, SDL2_mixer, SDL2_ttf and SDL2_net development headers are installed.
2. From the repository root, run:

```bash
cd src
make -f makefile.sdl2 tomenet
```

The resulting `tomenet` binary will appear in `src/`.

**Code style**

- Source files are written in C.
- Tabs are used for indentation.

**Testing**

There are currently no automated tests in this repository. Building the project is the main validation step.

