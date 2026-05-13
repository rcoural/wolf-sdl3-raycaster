// SDL3 callback-based entry point: SDL drives Init/Iterate/Event/Quit.
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL_main.h>

#include <memory>
#include <stdexcept>

#include "app.h"

extern "C" SDL_AppResult SDL_AppInit(void** appstate, int, char**) {
    try {
        auto app = std::make_unique<App>();
        if (!app->init()) {
            return SDL_APP_FAILURE;
        }
        *appstate = app.release();
        return SDL_APP_CONTINUE;
    } catch (const std::exception& e) {
        SDL_Log("Init failed: %s", e.what());
        return SDL_APP_FAILURE;
    }
}

extern "C" SDL_AppResult SDL_AppIterate(void* appstate) {
    try {
        auto* app = static_cast<App*>(appstate);
        app->update();
        app->render();
        SDL_Delay(16);
        return SDL_APP_CONTINUE;
    } catch (const std::exception& e) {
        SDL_Log("Iterate failed: %s", e.what());
        return SDL_APP_FAILURE;
    }
}

extern "C" SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* e) {
    try {
        auto* app = static_cast<App*>(appstate);
        app->handleEvent(*e);
        return app->isRunning() ? SDL_APP_CONTINUE : SDL_APP_SUCCESS;
    } catch (const std::exception& e) {
        SDL_Log("Event failed: %s", e.what());
        return SDL_APP_FAILURE;
    }
}

extern "C" void SDL_AppQuit(void* appstate, SDL_AppResult) {
    std::unique_ptr<App> app{static_cast<App*>(appstate)};
    if (app) {
        app->shutdown();
    }
}
