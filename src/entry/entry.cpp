#include <yaml-cpp/yaml.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>

#include "../application/application.h"
#include "../logger/logger.h"

namespace entix {

int main(int argc, char** argv) {
    Logger::info() << "Creating main application";
    Logger::endline();

    std::string configFile((argc > 1) ? argv[1] : "app.cfg");
    auto usingDefaultConfig = false;
    std::ostringstream ss;
    {
        std::ifstream cfg(configFile);
        if (cfg) {
            ss << cfg.rdbuf();
        } else {
            usingDefaultConfig = true;

            Logger::error() << configFile << " was not found";
            Logger::endline();

            Logger::info() << "Default configuration used";
            Logger::endline();
        }
    }

    YAML::Node node = YAML::Load(ss.str());
    std::string title = "Untitled";
    if (node["Title"]) title = node["Title"].as<std::string>();

    VectorI wSize(600, 400);
    if (node["Size"]) wSize = node["Size"].as<VectorI>();

    std::vector<core::Application::WindowFlag> windowFlags;
    if (node["Flags"]) {
        std::map<std::string, core::Application::WindowFlag> bind = {
            {"shown", core::Application::WindowFlag::SHOWN},
            {"resizable", core::Application::WindowFlag::RESIZABLE},
            {"minimized", core::Application::WindowFlag::MINIMIZED},
            {"maximized", core::Application::WindowFlag::MAXIMIZED},
            {"borderless", core::Application::WindowFlag::BORDERLESS},
            {"always on top", core::Application::WindowFlag::ALWAYS_ON_TOP},
            {"centered", core::Application::WindowFlag::CENTERED},
            {"fullscreen", core::Application::WindowFlag::FULLSCREEN},
            {"fullscreen desktop",
             core::Application::WindowFlag::FULLSCREEN_DESKTOP}};
        for (auto f : node["Flags"])
            windowFlags.push_back(bind[f.as<std::string>()]);
    }

    core::Application application(title, wSize.x, wSize.y, windowFlags);

    auto configPath = std::filesystem::path(configFile).parent_path();
    application._configPath = configPath.string();
    auto& serializer = application.getSerializer();

    if (node["Position"]) {
        auto n = node["Position"];
        if (n.IsSequence()) {
            auto pos = n.as<VectorI>();
            application.setWindowPosition(pos.x, pos.y);
        } else {
            Logger::warn() << "Position should be a sequence of two numbers";
            Logger::endline();
        }
    }

    if (node["FPS"]) application.setFramerate(node["FPS"].as<int>());

    auto scenesPath = configPath / core::Scene::FOLDER;
    if (!std::filesystem::exists(scenesPath) && !usingDefaultConfig) {
        Logger::warn() << "'scenes' folder not found in '" << configPath;
        Logger::endline();
    }

    auto oneSceneDeserialized = false;
    if (node["Scenes"]) {
        auto scenes = node["Scenes"].as<std::vector<std::string>>();
        if (!scenes.empty()) {
            core::SceneManager::Get()->setListOfScenes(scenes);
            oneSceneDeserialized = true;
        }
    }

    if (!oneSceneDeserialized) {
        Logger::error() << "No scene to run";
        Logger::endline();

        return 1;
    }

    application.run();

    Logger::info() << "Exiting application";
    Logger::endline();

    return 0;
}

}  // namespace entix