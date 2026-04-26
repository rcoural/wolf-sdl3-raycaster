// SDL3 callback-based entry point: SDL drives Init/Iterate/Event/Quit.
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL_main.h>

#include <memory>

#include "app.h"

extern "C" SDL_AppResult SDL_AppInit(void** appstate, int, char**) {
    auto app = std::make_unique<App>();
    if (!app->init()) {
        return SDL_APP_FAILURE;
    }
    *appstate = app.release();
    return SDL_APP_CONTINUE;
}

extern "C" SDL_AppResult SDL_AppIterate(void* appstate) {
    auto* app = static_cast<App*>(appstate);
    app->update();
    app->render();
    SDL_Delay(16);
    return SDL_APP_CONTINUE;
}

extern "C" SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* e) {
    auto* app = static_cast<App*>(appstate);
    app->handleEvent(*e);
    return app->isRunning() ? SDL_APP_CONTINUE : SDL_APP_SUCCESS;
}

extern "C" void SDL_AppQuit(void* appstate, SDL_AppResult) {
    std::unique_ptr<App> app{static_cast<App*>(appstate)};
    if (app) {
        app->shutdown();
    }
}
