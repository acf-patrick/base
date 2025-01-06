#include "application.h"

#include <SDL_ttf.h>

#include <cassert>

#include "../ecs/entity/entity.h"
#include "../ecs/system/system.h"
#include "../ecs/system/systems/systems.h"
#include "../event/event.h"
#include "../event/input.h"
#include "../logger/logger.h"
#include "../scene/scene.h"
#include "hook.h"

namespace entix {
namespace core {

Application* Application::instance = nullptr;
std::shared_ptr<Serializer> Application::_serializer;
std::shared_ptr<ApplicationHook> Application::_hook;

Application::Application(const std::string& title, int width, int height,
                         SDL_WindowFlags windowFlag)
    : _running(true) {
    if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
        log("SDL Error : initialisation of subsystems failed!");
        exit(EXIT_FAILURE);
    }

    _window =
        SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_CENTERED,
                         SDL_WINDOWPOS_CENTERED, width, height, windowFlag);
    if (!_window) {
        log("SDL Error : unable to create window");
        exit(EXIT_FAILURE);
    }

    SDL_Renderer* r =
        SDL_CreateRenderer(_window, -1, SDL_RENDERER_TARGETTEXTURE);
    if (!r) {
        log("SDL Error : unable to create a render context");
        exit(EXIT_FAILURE);
    }
    // SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

    RenderManager::Get()->renderer = r;
    auto renderer = RenderManager::Get()->renderer;

    initializeSystems();

    if (TTF_Init() < 0) {
        log("SDL Error : unable to initialize TTF library");
        exit(EXIT_FAILURE);
    }

    SDL_initFramerate(&_fpsManager);
    setFramerate(60);

    instance = this;

    Logger::info("App") << "Application created";
    Logger::endline();

    // running hook
    if (_hook) _hook->startup();
}

Application::~Application() {
    // running hook
    if (_hook) _hook->cleanup();

    IManager::DestroyInstances();

    // Make sure to free memory
    ecs::Entity::Clean();
    Texture::Clean();

    SDL_DestroyWindow(_window);
    _window = nullptr;

    TTF_Quit();
    SDL_Quit();

    Logger::dumpStatus(Logger::Status::ERROR, "error.log");
}

void Application::log(const std::string& message) const {
    Logger::error() << message << std::endl << SDL_GetError();
    Logger::endline();
}

void Application::run() {
    Uint32 latestTick = SDL_GetTicks();
    int frameCount = 0;

    while (_running) {
        auto currentTick = SDL_GetTicks();
        frameCount++;

        if (currentTick - latestTick >= 500) {
            _framerate = 1000 * frameCount / 500.0;
            frameCount = 0;
            latestTick = currentTick;
        }

        EventManager::Get()->handle();
        ecs::SystemManager::Get()->run();

        auto sceneManager = SceneManager::Get();
        if (!sceneManager->update())
            quit();  // No more scene left
        else
            sceneManager->render();

        RenderManager::Get()->render();
        SDL_framerateDelay(&_fpsManager);
    }
}

void Application::quit() { _running = false; }

void Application::setWindowPosition(int x, int y) {
    SDL_SetWindowPosition(_window, x, y);
}

std::filesystem::path Application::getConfigPath() { return _configPath; }

Serializer& Application::getSerializer() {
    if (!_serializer) {
        Logger::warn() << "No custom serializer set. Using default serializer.";
        Logger::endline();

        _serializer = std::make_shared<Serializer>();
    }
    return *_serializer;
}

void Application::setFramerate(unsigned int framerate) {
    if (SDL_setFramerate(&_fpsManager, framerate) < 0) {
        Logger::warn("App") << "Unable to set framerate to " << framerate;
        Logger::endline();
    }
}

int Application::getFramerate() const { return _framerate; }

int Application::getPreferredFramerate() const { return _fpsManager.rate; }

void Application::initializeSystems() {
    auto systems = ecs::SystemManager::Get();

#ifdef USE_PHYSIC_SYSTEM
    systems->add<ecs::system::PhysicSystem>();
#endif
}

// static
Application& Application::Get() {
    assert(instance && "Application has not been initialized yet");
    return *instance;
}

// static
void Application::Quit() {
    if (instance) instance->_running = false;
}

}  // namespace core
}  // namespace entix