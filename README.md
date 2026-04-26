# Wolf SDL3 Raycaster

A 2.5D Wolfenstein-style raycaster in C++23, using **SDL3** for windowing/input and the **SDL3 GPU API** for presentation.

The raycasting itself runs on the **CPU** (classic DDA: textured walls, floors and ceilings). Each frame is written into a CPU framebuffer, uploaded to a `SDL_GPUTexture`, and drawn as a fullscreen textured quad through an SDL3 GPU pipeline (SPIR-V on Vulkan, MSL on Metal, DXIL on D3D12).

Comes with a small tile-based level editor that reads/writes the same binary `.lvl` format.

## Features

- DDA raycaster with textured walls, floors and ceilings
- Multiple wall tile types (regular, secondary, entry, exit)
- CPU → GPU streaming via SDL3 GPU transfer buffers
- WASD + mouse-look movement with wall sliding
- Tile-based level editor (separate executable)
- Custom binary `.lvl` map format

## Controls

### Game

| Key                | Action          |
| ------------------ | --------------- |
| `W` / `↑`          | Move forward    |
| `S` / `↓`          | Move backward   |
| `A` / `D`          | Strafe          |
| `←` / `→` or mouse | Turn            |
| `Ctrl + Enter`     | Toggle fullscreen |
| `Esc`              | Quit            |

### Editor

| Key            | Action                |
| -------------- | --------------------- |
| Left click     | Paint selected tile   |
| Right click    | Erase tile            |
| Click palette  | Pick brush (top bar)  |
| `S`            | Save                  |
| `L`            | Reload                |
| `Esc`          | Quit                  |

`editor.exe [path/to/map.lvl] [--size WxH]`

## Build

Requires CMake ≥ 3.18 and a C++23 compiler (tested with GCC 13.1, MinGW). SDL3, SDL_shadercross and glm live as git submodules under `external/`.

```bash
git clone --recurse-submodules https://github.com/rcoural/wolf-sdl3-raycaster.git
cd wolf-sdl3-raycaster

cmake -B build
cmake --build build
```

If you forgot `--recurse-submodules`:

```bash
git submodule update --init --recursive
```

Outputs:

- `build/ProjectWolf` (or `.exe`) — the game
- `build/editor`     — the level editor

## Project layout

```
src/
  main.cpp        SDL3 callback entry point
  app.{h,cpp}     Window, GPU pipeline, per-frame loop
  game.{h,cpp}    World, tile grid, CPU raycaster
  player.{h,cpp}  Position, facing, WASD movement + collision
  editor/
    editor_main.cpp
    editor_app.{h,cpp}
    mapio.{h,cpp} Binary .lvl read/write
  shaders/        HLSL sources for the present pass
assets/           Textures (.bmp), maps (.lvl), compiled shader binaries
external/         SDL3, SDL_shadercross, glm (vendored)
```

## Map format (`.lvl`)

Tiny binary blob:

```
[u8 width][u8 height][width * height bytes of tile ids]
```

Tile ids:

| Id  | Meaning  |
| --- | -------- |
| 0   | Empty    |
| 1   | Wall     |
| 2   | Entry    |
| 3   | Exit     |
| 4   | Alt wall |

## License

MIT — see [LICENSE](LICENSE).

Vendored third-party code under `external/` keeps its own license:
- SDL3 — zlib
- SDL_shadercross — zlib
- glm — MIT
