// Tile-based level editor: top palette + grid below.
// Left click paints the selected tile, right click clears.
// Hotkeys: S = save, L = reload from disk, Esc = quit.

#include "editor_app.h"
#include "mapio.h"

#include <iostream>

bool EditorApp::init(const char* title, int width, int height,
                     const char* levelFilename, uint8_t defaultW, uint8_t defaultH) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
        return false;
    }

    currentFile = levelFilename;

    if (loadMap(currentFile, mapData, gridW, gridH)) {
        SDL_Log("Loaded map: %s (%ux%u)", currentFile.string().c_str(), gridW, gridH);
    }
    else {
        SDL_Log("No map loaded. Creating empty grid.");
        gridW = defaultW;
        gridH = defaultH;
        mapData.assign(static_cast<size_t>(gridW) * gridH, 0);
    }

    window = SDL_CreateWindow(title, width, height, 0);
    renderer = SDL_CreateRenderer(window, nullptr);

    cameraX      = 0;
    cameraY      = 32;
    selectedTile = 1;

    return true;
}

void EditorApp::run() {
    while (running) {
        handleEvents();
        render();
        SDL_Delay(16);
    }
}

void EditorApp::shutdown() {
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

void EditorApp::handleEvents() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_EVENT_QUIT) {
            running = false;
        }
        else if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
            const int mx = static_cast<int>(e.button.x);
            const int my = static_cast<int>(e.button.y);
            handleMouse(mx, my, e.button.button == SDL_BUTTON_LEFT);
        }
        else if (e.type == SDL_EVENT_KEY_DOWN) {
            if (e.key.key == SDLK_ESCAPE) running = false;
            if (e.key.key == SDLK_S)      save();
            if (e.key.key == SDLK_L)      load();
        }
    }
}

void EditorApp::handleMouse(int x, int y, bool leftClick) {
    // Top 32px is the tile palette; clicking it changes the active brush.
    if (y < 32) {
        const int index = x / 32;
        if (index >= 0 && index < 5) {
            selectedTile = index;
        }
        return;
    }

    // Below the palette is the grid; map screen coords to tile cell.
    const int cx = x / tileSize;
    const int cy = (y - 80) / tileSize;
    if (cx >= 0 && cx < gridW && cy >= 0 && cy < gridH) {
        mapData[static_cast<size_t>(cy) * gridW + cx] = leftClick ? selectedTile : 0;
    }
}

void EditorApp::render() {
    SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255);
    SDL_FRect panel{ 0, 0, 800, 32 };
    SDL_RenderFillRect(renderer, &panel);

    for (int i = 0; i < 5; ++i) {
        SDL_FRect rect{ static_cast<float>(i * 32), 0, 32, 32 };
        if (i == selectedTile) SDL_SetRenderDrawColor(renderer, 200, 200, 0, 255);
        else                   SDL_SetRenderDrawColor(renderer, 80 * i, 80 * i, 80 * i, 255);
        SDL_RenderFillRect(renderer, &rect);
    }

    SDL_SetRenderDrawColor(renderer, 90, 90, 90, 255);
    SDL_FRect viewport{ 0, 80, 800, 520 };
    SDL_RenderRect(renderer, &viewport);

    const int tileW = tileSize;
    const int tileH = tileSize;

    for (int y = 0; y < gridH; ++y) {
        for (int x = 0; x < gridW; ++x) {
            SDL_FRect rect{
                static_cast<float>(x * tileW - cameraX),
                static_cast<float>(y * tileH - cameraY + 32 + 80),
                static_cast<float>(tileW),
                static_cast<float>(tileH)
            };

            const uint8_t val = mapData[static_cast<size_t>(y) * gridW + x];

            switch (val) {
                case 0: SDL_SetRenderDrawColor(renderer,  50,  50,  50, 255); break;
                case 1: SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); break;
                case 2: SDL_SetRenderDrawColor(renderer,   0, 128, 255, 255); break;
                case 3: SDL_SetRenderDrawColor(renderer, 255,   0,   0, 255); break;
                case 4: SDL_SetRenderDrawColor(renderer,  25, 200,  50, 255); break;
                default: break;
            }
            SDL_RenderRect(renderer, &rect);
        }
    }

    SDL_RenderPresent(renderer);
}

void EditorApp::save() {
    if (!saveMap(currentFile, mapData, gridW, gridH)) {
        std::cerr << "Failed to save map\n";
    }
    else {
        std::cout << "Map saved.\n";
    }
}

void EditorApp::load() {
    if (!loadMap("map.lvl", mapData, gridW, gridH)) {
        std::cerr << "Failed to load map\n";
    }
    else {
        SDL_SetWindowSize(window, gridW * tileSize, gridH * tileSize);
        std::cout << "Map loaded.\n";
    }
}
