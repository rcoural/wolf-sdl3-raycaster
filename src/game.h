#pragma once

// World, map and CPU-side raycaster.
// Game owns the player, the tile grid and the wall/floor/ceiling textures.

#include <SDL3/SDL.h>

#include <cstdint>
#include <memory>
#include <numbers>
#include <span>
#include <string>
#include <vector>

#include "player.h"

inline constexpr int   TILE     = 64;
inline constexpr int   WIN_W    = 800;
inline constexpr int   WIN_H    = 600;
inline constexpr int   TEX_SIZE = 64;
inline constexpr float PI       = std::numbers::pi_v<float>;

// Map tile values stored in .lvl files; 0 = walkable, >0 = solid.
enum class TileType : uint8_t {
    Empty       = 0,
    Wall        = 1,
    Entry       = 2,
    Exit        = 3,
    WallAlt     = 4,
    OutOfBounds = 255,
};

// RAII wrapper for SDL_Surface.
struct SDLSurfaceDeleter {
    void operator()(SDL_Surface* surface) const noexcept {
        if (surface) SDL_DestroySurface(surface);
    }
};
using SurfacePtr = std::unique_ptr<SDL_Surface, SDLSurfaceDeleter>;

class Game {
public:
    Game();

    bool loadAssets(const std::string& path);
    void unloadAssets();

    void handleInput(std::span<const bool> keys);
    void handleMouseMotion(float xrel);

    void castRays(uint32_t* buffer, int pitchPixels, int outW, int outH);

private:
    Player player;

    std::vector<uint8_t> mapData;
    uint8_t gridW = 8;
    uint8_t gridH = 18;

    SurfacePtr entrySurface;
    SurfacePtr exitSurface;
    SurfacePtr wallSurface;
    SurfacePtr wallAltSurface;
    SurfacePtr floorSurface;
    SurfacePtr ceilingSurface;

    [[nodiscard]] uint8_t isWall(float x, float y) const;

    friend class Player;
};
