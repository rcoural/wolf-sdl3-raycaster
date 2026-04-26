#include "game.h"
#include "app.h"
#include "editor/mapio.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <iostream>

namespace {

SurfacePtr loadAndConvert(const std::string& path) {
    SurfacePtr raw{ SDL_LoadBMP(path.c_str()) };
    if (!raw) return nullptr;
    return SurfacePtr{ SDL_ConvertSurface(raw.get(), SDL_PIXELFORMAT_ARGB8888) };
}

} // namespace

Game::Game() {
    // Load level from disk; fall back to a solid 8x18 block if missing.
    uint8_t w = 0;
    uint8_t h = 0;
    std::vector<uint8_t> flat;
    if (loadMap(std::format("{}/assets/map.lvl", App::BasePath), flat, w, h)) {
        gridW   = w;
        gridH   = h;
        mapData = std::move(flat);
    }
    else {
        std::cerr << "Failed to load map.lvl, using fallback.\n";
        gridW = 8;
        gridH = 18;
        mapData.assign(static_cast<size_t>(gridW) * gridH, 1);
    }
}

bool Game::loadAssets(const std::string& path) {
    floorSurface    = loadAndConvert(std::format("{}/assets/floor.bmp",     path));
    ceilingSurface  = loadAndConvert(std::format("{}/assets/ceiling.bmp",   path));
    wallSurface     = loadAndConvert(std::format("{}/assets/wall.bmp",      path));
    wallAltSurface  = loadAndConvert(std::format("{}/assets/wall_alt.bmp",  path));
    entrySurface    = loadAndConvert(std::format("{}/assets/stairup.bmp",   path));
    exitSurface     = loadAndConvert(std::format("{}/assets/stairdown.bmp", path));

    return wallSurface && floorSurface && ceilingSurface;
}

void Game::unloadAssets() {
    wallSurface.reset();
    floorSurface.reset();
    ceilingSurface.reset();
    wallAltSurface.reset();
    entrySurface.reset();
    exitSurface.reset();
}

void Game::handleInput(std::span<const bool> keys) {
    player.handleInput(keys, *this);
}

void Game::handleMouseMotion(float xrel) {
    player.handleMouseMotion(xrel);
}

// World coords -> tile id; returns OutOfBounds outside the map.
uint8_t Game::isWall(float x, float y) const {
    const int mx = static_cast<int>(x) / TILE;
    const int my = static_cast<int>(y) / TILE;
    if (mx < 0 || mx >= gridW || my < 0 || my >= gridH) {
        return static_cast<uint8_t>(TileType::OutOfBounds);
    }
    return mapData[static_cast<size_t>(my) * gridW + mx];
}

// CPU raycaster: writes one column of pixels per output pixel, top-to-bottom.
// `buffer` is an ARGB image of `outW * outH` with row stride `pitchPixels`.
void Game::castRays(uint32_t* buffer, int pitchPixels, int outW, int outH) {
    constexpr float fov = 60.0f;
    const float px    = player.getX();
    const float py    = player.getY();
    const float angle = player.getAngle();

    const auto* floorPixels   = static_cast<const uint32_t*>(floorSurface->pixels);
    const auto* ceilingPixels = static_cast<const uint32_t*>(ceilingSurface->pixels);
    const auto* wallPixels    = static_cast<const uint32_t*>(wallSurface->pixels);
    const auto* wallAltPixels = static_cast<const uint32_t*>(wallAltSurface->pixels);
    const auto* entryPixels   = static_cast<const uint32_t*>(entrySurface->pixels);
    const auto* exitPixels    = static_cast<const uint32_t*>(exitSurface->pixels);

    // Camera basis: dir is forward, plane is the perpendicular half-FOV vector.
    const float posX     = px / TILE;
    const float posY     = py / TILE;
    const float angleRad = angle * PI / 180.0f;
    const float dirX     = std::cos(angleRad);
    const float dirY     = std::sin(angleRad);
    const float planeX   = -dirY * 0.66f;
    const float planeY   =  dirX * 0.66f;

    for (int ray = 0; ray < outW; ++ray) {
        // cameraX in [-1, +1] across the screen -> ray direction for this column.
        const float cameraX = 2.0f * ray / static_cast<float>(outW) - 1.0f;
        const float rayDirX = dirX + planeX * cameraX;
        const float rayDirY = dirY + planeY * cameraX;

        // --- DDA: step from cell to cell along the ray until we hit a wall. ---
        int mapX = static_cast<int>(posX);
        int mapY = static_cast<int>(posY);

        const float deltaDistX = (rayDirX == 0) ? 1e30f : std::fabs(1.0f / rayDirX);
        const float deltaDistY = (rayDirY == 0) ? 1e30f : std::fabs(1.0f / rayDirY);

        const int stepX = (rayDirX < 0) ? -1 : 1;
        const int stepY = (rayDirY < 0) ? -1 : 1;

        float sideDistX = (rayDirX < 0)
            ? (posX - mapX) * deltaDistX
            : (mapX + 1.0f - posX) * deltaDistX;

        float sideDistY = (rayDirY < 0)
            ? (posY - mapY) * deltaDistY
            : (mapY + 1.0f - posY) * deltaDistY;

        uint8_t wallType = 0;
        bool hit  = false;
        bool side = false; // false = vertical wall hit, true = horizontal

        while (!hit) {
            if (sideDistX < sideDistY) {
                sideDistX += deltaDistX;
                mapX      += stepX;
                side       = false;
            }
            else {
                sideDistY += deltaDistY;
                mapY      += stepY;
                side       = true;
            }
            wallType = isWall(mapX * TILE, mapY * TILE);
            hit      = wallType > 0;
        }

        // Perpendicular distance to wall, then fish-eye correction with cos(angleDiff).
        const float dist = side ? (sideDistY - deltaDistY) : (sideDistX - deltaDistX);

        const float rx = posX + rayDirX * dist;
        const float ry = posY + rayDirY * dist;

        const float dx = rx - posX;
        const float dy = ry - posY;
        const float realDist = std::sqrt(dx * dx + dy * dy);

        const float angleDiff = cameraX * (fov / 2.0f);
        const float corrected = realDist * std::cos(angleDiff * PI / 180.0f);

        // Projected wall column: vertical span of pixels in the output image.
        const int h          = static_cast<int>((outH * 0.5f) / (corrected + 0.0001f));
        const int wallTop    = std::max(0, outH / 2 - h);
        const int wallBottom = std::min(outH - 1, outH / 2 + h);

        // --- Floor: project each row below the wall back to a world position
        //     and sample the floor texture at the wrap-around tile coords.
        for (int py = outH - 1; py > wallBottom; --py) {
            const float rowDist = (0.5f * outH) / (py - (outH * 0.5f));
            const float worldX  = posX + rowDist * rayDirX;
            const float worldY  = posY + rowDist * rayDirY;

            const int texX = static_cast<int>(TEX_SIZE * (worldX - static_cast<int>(worldX))) & (TEX_SIZE - 1);
            const int texY = static_cast<int>(TEX_SIZE * (worldY - static_cast<int>(worldY))) & (TEX_SIZE - 1);

            buffer[py * pitchPixels + ray] = floorPixels[texY * TEX_SIZE + texX];
        }

        // --- Ceiling: same trick, mirrored above the wall.
        for (int py = 0; py < wallTop; ++py) {
            const float rowDist = (0.5f * outH) / ((outH * 0.5f) - py);
            const float worldX  = posX + rowDist * rayDirX;
            const float worldY  = posY + rowDist * rayDirY;

            const int texX = static_cast<int>(TEX_SIZE * (worldX - static_cast<int>(worldX))) & (TEX_SIZE - 1);
            const int texY = static_cast<int>(TEX_SIZE * (worldY - static_cast<int>(worldY))) & (TEX_SIZE - 1);

            buffer[py * pitchPixels + ray] = ceilingPixels[texY * TEX_SIZE + texX];
        }

        // --- Wall: pick texture by tile id, sample column by hit position,
        //     then stretch vertically across [wallTop, wallBottom].
        const uint32_t* texturePixels = wallPixels;
        switch (static_cast<TileType>(wallType)) {
            case TileType::Wall:    texturePixels = wallPixels;    break;
            case TileType::Entry:   texturePixels = entryPixels;   break;
            case TileType::Exit:    texturePixels = exitPixels;    break;
            case TileType::WallAlt: texturePixels = wallAltPixels; break;
            default: break;
        }

        const float offset = std::fabs(rx - std::floor(rx)) + std::fabs(ry - std::floor(ry));
        const int   texX   = static_cast<int>(offset * TEX_SIZE) & (TEX_SIZE - 1);

        for (int ypix = wallTop; ypix < wallBottom; ++ypix) {
            int texY = ((ypix - static_cast<int>((outH * 0.5f) - h)) * TEX_SIZE) / (h * 2);
            texY = std::clamp(texY, 0, TEX_SIZE - 1);
            buffer[ypix * pitchPixels + ray] = texturePixels[texY * TEX_SIZE + texX];
        }
    }
}
