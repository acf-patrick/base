#include "serializer.h"

#include <cstdlib>
#include <fstream>
#include <regex>
#include <sstream>

#include "../application/application.h"
#include "../ecs/components.h"
#include "../logger/logger.h"
#include "../path/path.h"
#include "../renderer/renderer.h"
#include "../task/asynchronous.h"
#include "../task/sequential.h"
#include "../task/task_pool.h"
#include "../util/exprtk/exprtk.hpp"

namespace entix {
namespace core {

Scene *Serializer::deserialize(const std::string &sceneName) try {
    auto error = [](const std::string &message) -> Scene * {
        Logger::error("Deserializer")
            << "Invalid file format! Cause: " << message;
        Logger::endline();

        return nullptr;
    };

    Path source;
    source = source / (Scene::FOLDER + "/" + sceneName + Scene::FILE_EXTENSION);

    std::ifstream file(source);

    if (!file) {
        Logger::error("Deserializer")
            << "Scene file : " << source << " doesn't exist!";
        Logger::endline();

        return nullptr;
    }

    std::stringstream ss;
    ss << file.rdbuf();
    YAML::Node node = YAML::Load(ss.str());

    auto name = node["Name"];
    if (!name) return error("'Name' node was not found");

    Scene *scene = new Scene(name.as<std::string>());

    auto entities = node["Entities"];
    if (!entities) return error("'Entities' node was not found");

    for (auto entity : entities)
        if (entity["ID"])
            deserializeEntity(
                entity, scene->_entities.create(entity["ID"].as<EntityID>()));
        else
            // Let ecs::Entity class create an ID if there's no ID node
            deserializeEntity(entity, scene->_entities.create());

    // Make sure scene has at least one camera
    if (!scene->_entities["main camera"])
        scene->_entities.create("main camera").attach<ecs::component::Camera>();
    else {
        auto &camera = *scene->_entities["main camera"];
        if (!camera.has<ecs::component::Camera>())
            camera.attach<ecs::component::Camera>();
    }

    // fetch systems
    if (node["Systems"]) {
        auto systems = node["Systems"];
        if (!deserializeSystems(systems, *scene)) {
            Logger::error("Deserializer")
                << "Unable to deserialize systems for scene '" << scene->tag;
            Logger::endline();
        }
    }

    if (scene)
        Logger::info("Deserializer") << source << " loaded";
    else
        Logger::error("Deserializer") << "Failed to load " << source;
    Logger::endline();

    return scene;
} catch (std::exception &e) {
    Logger::error("Deserializer")
        << "Unable to deserialize scene. Cause: " << e.what();
    Logger::endline();
    return nullptr;
}

void Serializer::serialize(Scene *scene, const std::string &fileName) {
    if (!scene) {
        Logger::error("Serializer") << "NULL scene : Failed to serialize!";
        Logger::endline();

        return;
    }

    YAML::Emitter out;
    out << YAML::BeginMap;
    out << YAML::Key << "Name" << YAML::Value << scene->tag;

    out << YAML::Key << "Entities" << YAML::Value;

    out << YAML::BeginSeq;
    scene->_entities.for_each([&](ecs::Entity &entity) {
        out << YAML::BeginMap;
        serializeEntity(out, entity);
        out << YAML::EndMap;
    });
    out << YAML::EndSeq;

    out << YAML::EndMap;

    auto outputFilename = (fileName.empty() ? (scene->tag + ".scn") : fileName);
    auto sceneFolderPath = Application::Get().getConfigPath() / "scenes";
    auto folderCreationOk = false;

    try {
        if (!fs::exists(sceneFolderPath) &&
            !fs::create_directories(sceneFolderPath)) {
            Logger::warn("Serializer")
                << "Unable to create scene folder. Path : '" << sceneFolderPath
                << "'";
            Logger::endline();
        } else
            folderCreationOk = true;
    } catch (std::exception &e) {
        Logger::error("Serializer")
            << "Unable to serialize scene. Cause: " << e.what();
        Logger::endline();
    }

    if (folderCreationOk) {
        auto filename = sceneFolderPath / outputFilename;
        std::ofstream file(filename);
        file << out.c_str();

        Logger::info("Serializer")
            << "Scene serialized to '" << filename << "'";
        Logger::endline();
    }
}

bool Serializer::_deserializeTask(std::shared_ptr<task::ITask> &task,
                                  YAML::Node &nodes, ecs::Entity &entity) {
    if (!nodes.IsSequence()) {
        Logger::error("Deserializer")
            << "Task node must be a sequence of task name or another node of "
               "async/seq task";
        Logger::endline();
        return false;
    }

    auto displayErrorAt = [](const YAML::Node &node) {
        auto mark = node.Mark();
        Logger::error("Deserializer")
            << "Unable to deserialize task at [" << mark.line << ", "
            << mark.column << "]";
        Logger::endline();
    };

    for (auto node : nodes) {
        if (node.IsScalar()) {
            if (auto customTask = deserializeTask(node, entity); customTask)
                task->push(customTask);
            else {
                displayErrorAt(node);
                return false;
            }

        } else if (node.IsMap()) {
            auto asyncNode = node["Asynchronous"];
            auto seqNode = node["Sequential"];
            auto isCustomTask = false;

            if (asyncNode && seqNode) {
                Logger::error("Deserializer")
                    << "Task can not be asynchronous and sequential at the "
                       "same time";
                Logger::endline();
                return false;
            }

            std::shared_ptr<task::ITask> newTask;
            if (asyncNode)
                newTask = std::make_shared<task::AsynchronousTask>();
            else if (seqNode)
                newTask = std::make_shared<task::SequentialTask>();
            else if (auto customTask = deserializeTask(node, entity);
                     customTask) {
                newTask = customTask;
                isCustomTask = true;
            } else {
                displayErrorAt(node);
                return false;
            }

            if (isCustomTask)
                task->push(newTask);
            else if (_deserializeTask(newTask, asyncNode ? asyncNode : seqNode,
                                      entity))  // has encountered either async
                                                // or sequential task
                task->push(newTask);
            else
                return false;

        } else {
            Logger::error("Deserializer")
                << "Task item must be either a user defined task-name or "
                   "another node of async/seq task";
            Logger::endline();
            return false;
        }
    }

    return true;
}

std::shared_ptr<task::ITask> Serializer::deserializeTask(const YAML::Node &node,
                                                         ecs::Entity &entity) {
    return nullptr;
}

bool Serializer::deserializeTasks(YAML::Node &node, ecs::Entity &entity) {
    if (!node.IsMap()) {
        Logger::error("Deserializer") << "Root task must be a mapping.";
        Logger::endline();
        return false;
    }

    auto asyncNode = node["Asynchronous"];
    auto seqNode = node["Sequential"];

    if (asyncNode && seqNode) {
        Logger::error("Deserializer") << "'Tasks' can only have one root node.";
        Logger::endline();
        return false;
    }

    if (!asyncNode && !seqNode) {
        Logger::error("Deserializer") << "Invalid Task. Root task should be "
                                         "either Asynchronous or Sequential";
        Logger::endline();
        return false;
    }

    std::shared_ptr<task::ITask> rootTask;
    if (asyncNode)
        rootTask = std::make_shared<task::AsynchronousTask>();
    else
        rootTask = std::make_shared<task::SequentialTask>();

    if (_deserializeTask(rootTask, asyncNode ? asyncNode : seqNode, entity)) {
        entity.attach<task::TaskPool>(rootTask);
        return true;
    }

    return false;
}

bool Serializer::deserializeSystems(YAML::Node &node, Scene &scene) {
    if (!node.IsSequence()) {
        Logger::error("Deserializer")
            << "'Systems' should be a list of system names";
        Logger::endline();
        return false;
    }

    auto i = 0;
    for (auto systems : node) {
        if (systems.IsScalar()) {
            scene._freeSystems.push_back(systems.as<std::string>());
        } else if (systems.IsSequence()) {
            scene._systemGroups.push_back(
                systems.as<std::vector<std::string>>());
        } else {
            Logger::warn("Deserializer") << "Invalid system node at index " << i
                                         << ". System node should be a string "
                                            "or sequence of system names";
            Logger::endline();
        }
        i++;
    }

    return true;
}

void Serializer::deserializeEntity(YAML::Node &node, ecs::Entity &entity) {
    YAML::Node n;
    n = node["Template"];
    if (n) {
        if (n.IsSequence())
            for (auto childNode : n)
                entity.useTemplate(childNode.as<std::string>());
        else
            entity.useTemplate(n.as<std::string>());
    }

    n = node["Tasks"];
    if (n) {
        if (!deserializeTasks(n, entity)) {
            Logger::error("Deserializer")
                << "Unable to deserialize tasks correctly please ensure you "
                   "have overriden Serializer::deseriliazeTask for your own "
                   "specific task";
            Logger::endline();
        }
    }

    {  // deserializing components
        n = node["TagComponent"];
        if (n) entity.attach<ecs::component::Tag>(n["Tag"].as<std::string>());

        n = node["TransformComponent"];
        if (n) {
            auto &t = entity.attach<ecs::component::Transform>();
            if (n["Position"]) t.position = parseVector<double>(n["Position"]);
            if (n["Scale"]) t.scale = n["Scale"].as<VectorF>();
            if (n["Rotation"]) t.rotation = n["Rotation"].as<double>();
        }

        n = node["SpriteComponent"];
        if (n) {
            auto &s = entity.attach<ecs::component::Sprite>();
            if (n["Texture"]) s.texture.load(n["Texture"].as<std::string>());
            if (n["Centered"]) s.centered = n["Centered"].as<bool>();
            if (n["Offset"]) s.offset = n["Offset"].as<VectorI>();
            if (n["Flip"]) s.flip = n["Flip"].as<Vector<bool>>();
            if (n["FramesNumber"])
                s.framesNumber = n["FramesNumber"].as<VectorI>();
            if (n["Frame"]) s.frame = n["Frame"].as<int>();
            if (n["RegionEnabled"])
                s.regionEnabled = n["RegionEnabled"].as<bool>();
            if (n["Region"]) s.region = n["Region"].as<SDL_Rect>();
        }

        n = node["SpriteRendererComponent"];
        if (n) entity.attach<ecs::component::SpriteRenderer>();

        n = node["CameraComponent"];
        if (n) {
            auto &c = entity.attach<ecs::component::Camera>();
            if (n["Size"]) c.size = n["Size"].as<VectorF>();
            if (n["Destination"])
                c.destination = n["Destination"].as<VectorF>();
            if (n["BackgroundColor"])
                c.background = n["BackgroundColor"].as<SDL_Color>();
            if (n["BackgroundImage"])
                c.backgroundImage.load(n["BackgroundImage"].as<std::string>());
            if (n["ClearMode"]) {
                std::map<std::string, ecs::component::Camera::ClearMode> bind =
                    {{"none", ecs::component::Camera::ClearMode::NONE},
                     {"texture", ecs::component::Camera::ClearMode::TEXTURE},
                     {"solid color",
                      ecs::component::Camera::ClearMode::SOLID_COLOR},
                     {"texture and solid color",
                      ecs::component::Camera::ClearMode::
                          TEXTURE_AND_SOLID_COLOR}};
                c.clear = bind[n["ClearMode"].as<std::string>()];
            }
            if (n["Flip"]) c.flip = n["Flip"].as<Vector<bool>>();
            if (n["Depth"]) c.depth = n["Depth"].as<int>();
            if (n["Layers"]) c.layers = n["Layers"].as<std::vector<int>>();
        }

        n = node["Tilemap"];
        if (n) entity.attach<ecs::component::Tilemap>(n.as<std::string>());

        n = node["SpriteAnimatorComponent"];
        if (n)
            entity.attach<ecs::component::animation::SpriteAnimator>(
                n["FrameDuration"].as<int>());
    }
}

void Serializer::serializeEntity(YAML::Emitter &out, ecs::Entity &entity) {
    out << YAML::Key << "ID" << YAML::Value << entity.idAsString();

    if (entity.has<ecs::component::Tag>()) {
        out << YAML::Key << "TagComponent" << YAML::Value;
        out << YAML::BeginMap;
        out << YAML::Key << "Tag" << YAML::Value
            << entity.get<ecs::component::Tag>().tag;
        out << YAML::EndMap;
    }

    if (entity.has<ecs::component::Transform>()) {
        out << YAML::Key << "TransformComponent" << YAML::Value;
        auto &t = entity.get<ecs::component::Transform>();
        out << YAML::BeginMap;
        out << YAML::Key << "Position" << YAML::Value << t.position;
        out << YAML::Key << "Scale" << YAML::Value << t.scale;
        out << YAML::Key << "Rotation" << YAML::Value << t.rotation;
        out << YAML::EndMap;
    }

    if (entity.has<ecs::component::Sprite>()) {
        out << YAML::Key << "SpriteComponent" << YAML::Value;
        auto &s = entity.get<ecs::component::Sprite>();
        out << YAML::BeginMap;
        out << YAML::Key << "Texture" << YAML::Value << s.texture.getName();
        out << YAML::Key << "Centered" << YAML::Value << s.centered;
        out << YAML::Key << "Offset" << YAML::Value << s.offset;
        out << YAML::Key << "Flip" << YAML::Value << s.flip;
        out << YAML::Key << "FramesNumber" << YAML::Value << s.framesNumber;
        out << YAML::Key << "Frame" << YAML::Value << s.frame;
        out << YAML::Key << "RegionEnabled" << YAML::Value << s.regionEnabled;
        out << YAML::Key << "Region" << YAML::Value << s.region;
        out << YAML::EndMap;
    }

    // using map in case SpriteRenderer has propreties in the futur
    if (entity.has<ecs::component::SpriteRenderer>())
        out << YAML::Key << "SpriteRendererComponent"
            << YAML::Comment("Native Script derived class");

    if (entity.has<ecs::component::Camera>()) {
        out << YAML::Key << "CameraComponent" << YAML::Value;
        auto &c = entity.get<ecs::component::Camera>();
        out << YAML::BeginMap;
        out << YAML::Key << "Size" << YAML::Value << c.size;
        out << YAML::Key << "Destination" << YAML::Value << c.destination;
        out << YAML::Key << "BackgroundColor" << YAML::Value << c.background;
        out << YAML::Key << "BackgroundImage" << YAML::Value
            << c.backgroundImage.getName();
        std::string bind[] = {"none", "texture", "solid Color",
                              "texture and solid color"};
        out << YAML::Key << "ClearMode" << YAML::Value << bind[(int)c.clear];
        out << YAML::Key << "Flip" << YAML::Value << c.flip;
        out << YAML::Key << "Depth" << YAML::Value << c.depth;
        out << YAML::Key << "Layers" << YAML::Value << YAML::Flow << c.layers;
        out << YAML::EndMap;
    }

    if (entity.has<ecs::component::Tilemap>()) {
        auto &map = entity.get<ecs::component::Tilemap>();
        out << YAML::Key << "Tilemap" << YAML::Value << map.getSource();
    }

    if (entity.has<ecs::component::animation::SpriteAnimator>()) {
        out << YAML::Key << "SpriteAnimatorComponent" << YAML::Value;
        out << YAML::BeginMap;
        out << YAML::Key << "FrameDuration" << YAML::Value
            << entity.get<ecs::component::animation::SpriteAnimator>()
                   .getFrameDuration();
        out << YAML::EndMap;
    }
}

template <typename T>
Vector<T> Serializer::parseVector(const YAML::Node &node) const {
    if (node.IsScalar()) {
        const auto value = node.as<std::string>();
        std::regex functionRegex(
            R"(normalizedWindowCoords\(([-+]?[0-9]*\.?[0-9]+)\s*,\s*([-+]?[0-9]*\.?[0-9]+)\))");

        std::smatch match;
        if (std::regex_match(value, match, functionRegex)) {
            const auto x = std::stof(match[1].str());
            const auto y = std::stof(match[2].str());
            return unormalizeWindowCoordinates<T>(x, y);
        }
    } else if (node.IsSequence() && node.size() == 2) {
        auto isNodeANumber = [](const YAML::Node &node) {
            try {
                auto _ = node.as<double>();
                return true;
            } catch (...) {
                return false;
            }
        };

        if (isNodeANumber(node[0]) && isNodeANumber(node[1]))
            return node.as<Vector<T>>();

        const auto variables = getDefinedVariables();
        return {evaluateExpression(node[0].as<std::string>(), variables),
                evaluateExpression(node[1].as<std::string>(), variables)};
    }

    std::stringstream ss;
    const auto nodeMark = node.Mark();
    ss << "Invalid Position format at [" << nodeMark.line << ", "
       << nodeMark.column << ']';

    throw std::runtime_error(ss.str());
}

template <typename T>
T Serializer::evaluateExpression(
    const std::string &expr,
    const std::unordered_map<std::string, T> &variables) const {
    exprtk::symbol_table<T> symbolTable;
    for (auto &[key, value] : variables) {
        symbolTable.add_constant(key, value);
    }

    exprtk::parser<T> parser;
    const auto expression = parser.compile(expr, symbolTable);
    return expression.value();
}

template <typename T>
Vector<T> Serializer::unormalizeWindowCoordinates(float x, float y) const {
    const auto windowSize = RenderManager::Get()->getSize();
    return {windowSize.x * x, windowSize.y * y};
}

std::unordered_map<std::string, double> Serializer::getDefinedVariables()
    const {
    const auto windowSize = RenderManager::Get()->getSize();
    return {{"WINDOW_WIDTH", windowSize.x}, {"WINDOW_HEIGHT", windowSize.y}};
}

int Serializer::_cnt = 0;
Serializer::Serializer() {
    assert(!_cnt && "Serializer can't have more than one instance!");
    _cnt++;
}

}  // namespace core
}  // namespace entix