#include "editor_app.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

int main(int argc, char** argv) {
    const char* filename = "map.lvl"; // default

    uint8_t mapW = 50;
    uint8_t mapH = 30;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--size") == 0 && i + 1 < argc) {
            unsigned w = 0;
            unsigned h = 0;
            if (std::sscanf(argv[i + 1], "%ux%u", &w, &h) == 2) {
                mapW = static_cast<uint8_t>(w);
                mapH = static_cast<uint8_t>(h);
                ++i;
            }
        }
        else {
            filename = argv[i];
        }
    }

    EditorApp app;
    if (!app.init("Level Editor", 800, 600, filename, mapW, mapH)) return 1;
    app.run();
    app.shutdown();
    return 0;
}
