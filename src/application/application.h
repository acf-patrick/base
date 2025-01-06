/**
 * @author acf-patrick (miharisoap@gmail.com)
 *
 * Define application main class
 */

#pragma once

#include <SDL.h>
#include <SDL2_framerate.h>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "../ecs/system/system.h"
#include "../entry/entry.h"
#include "../renderer/renderer.h"
#include "../serializer/serializer.h"

namespace entix {
namespace core {

class ApplicationHook;

class Application final {
   public:
    enum class WindowFlag {
        SHOWN,
        RESIZABLE,
        MINIMIZED,
        MAXIMIZED,
        BORDERLESS,
        ALWAYS_ON_TOP,
        CENTERED,
        FULLSCREEN_DESKTOP,
        FULLSCREEN
    };

    ~Application();

    void run();

    void quit();
    void log(const std::string&) const;

    std::filesystem::path getConfigPath();

    // returns realtime FPS
    int getFramerate() const;

    // returns FPS from configurations
    int getPreferredFramerate() const;

    template <typename TSerializer>
    static void setSerializer() {
        _serializer = std::make_shared<TSerializer>();
    }

    template <typename Hook>
    static void setup() {
        _hook = std::make_shared<Hook>();
    }

    Serializer& getSerializer();

    static void Quit();

    static Application& Get();

   private:
    Application(const std::string&, int, int,
                const std::vector<WindowFlag>& windowFlags);

    void setWindowPosition(int, int);

    void setFramerate(unsigned int);

    void initializeSystems();

    std::optional<SDL_WindowFlags> windowFlagToSDLWindowFlag(WindowFlag flag);

    bool _running = true;
    SDL_Window* _window;
    std::string _configPath;
    FPSmanager _fpsManager;
    int _framerate = 0;

    static std::shared_ptr<Serializer> _serializer;

    static std::shared_ptr<ApplicationHook> _hook;

    static Application* instance;

    friend int entix::main(int, char**);
};

}  // namespace core
}  // namespace entix