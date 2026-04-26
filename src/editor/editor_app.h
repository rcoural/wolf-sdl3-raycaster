#pragma once

#include <SDL3/SDL.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

class EditorApp {
public:
    EditorApp() = default;
    ~EditorApp() = default;

    EditorApp(const EditorApp&)            = delete;
    EditorApp& operator=(const EditorApp&) = delete;
    EditorApp(EditorApp&&)                 = delete;
    EditorApp& operator=(EditorApp&&)      = delete;

    bool init(const char* title, int width, int height,
              const char* levelFilename, uint8_t defaultW, uint8_t defaultH);
    void run();
    void shutdown();

private:
    SDL_Window*   window   = nullptr;
    SDL_Renderer* renderer = nullptr;
    bool          running  = true;

    std::vector<uint8_t> mapData;
    uint8_t gridW    = 8;
    uint8_t gridH    = 18;
    int     tileSize = 16;

    int cameraX      = 0;
    int cameraY      = 32;
    int selectedTile = 1;

    std::filesystem::path currentFile;

    void render();
    void handleEvents();
    void handleMouse(int x, int y, bool leftClick);
    void save();
    void load();
};
