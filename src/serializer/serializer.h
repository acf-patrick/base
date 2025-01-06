/**
 * @author acf-patrick (miharisoap@gmail.com)
 *
 * Class used for reading and writing datas
 */

#pragma once

#include <yaml-cpp/yaml.h>

#include <memory>

#include "../ecs/entity/entity.h"
#include "../scene/scene.h"
#include "../task/task.h"
#include "specializations.h"

namespace entix {
namespace core {

class Application;

/**
 * Serializer class
 * override serialize/desrialize-Entity for more functionality
 * Application::serializer to use unique instance of this class
 */
class Serializer {
    static int _cnt;

    bool _deserializeTask(std::shared_ptr<task::ITask>& task, YAML::Node&,
                          ecs::Entity&);

   protected:
    // Throws an exception if error occures
    template <typename T>
    T evaluateExpression(
        const std::string& expr,
        const std::unordered_map<std::string, T>& variables) const;

    template <typename T>
    Vector<T> unormalizeWindowCoordinates(float x, float y) const;

    /// @brief Parse a node as a vector
    ///
    /// Parse a node with support for function and macros as value and not just
    /// a pair of numbers
    template <typename T>
    Vector<T> parseVector(const YAML::Node& node) const;

    virtual std::unordered_map<std::string, double> getDefinedVariables() const;

   public:
    Serializer();

    virtual ~Serializer() = default;

    virtual void serialize(Scene*, const std::string& fileName = "");
    virtual Scene* deserialize(const std::string& sceneName);

    // Serialize entity
    virtual void serializeEntity(YAML::Emitter&, ecs::Entity&);

    /**
     * Deserialize with your own task type
     * @param node task node
     * @param entity entity the task pool will be attached to
     */
    virtual std::shared_ptr<task::ITask> deserializeTask(const YAML::Node& node,
                                                         ecs::Entity& entity);

    bool deserializeTasks(YAML::Node&, ecs::Entity&);

    // Fetch system names
    bool deserializeSystems(YAML::Node&, Scene&);

    // Deserialize entity
    virtual void deserializeEntity(YAML::Node&, ecs::Entity&);

    friend class Application;
};

}  // namespace core
}  // namespace entix