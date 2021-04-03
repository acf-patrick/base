#pragma once

#include <iostream>
#include <unordered_map>
#include <memory>
#include "componentArray.h"
#include "../defs.h"

class Entity;

class ComponentManager
{
private:

	ComponentManager()
	{ instance = this; }

    template<typename T>
    ComponentTypeID getComponentTypeID()
    {
        const char* typeName = typeid(T).name();

        if(_componentTypeID.find(typeName) == std::end(_componentTypeID))
            registerComponent<T>();

        return _componentTypeID[typeName];
    }

    template<typename T>
    void addComponent(EntityID e, T* component)
    {
        getComponentArray<T>()->insertData(e, component);
    }

    template<typename T>
    void removeComponent(EntityID e)
    {
        getComponentArray<T>()->removeData(e);
    }

    template<typename T>
    T& getComponent(EntityID e)
    {
        return getComponentArray<T>()->getData(e);
    }

    void entityDestroyed(EntityID);

    static ComponentManager& get();

    template<typename T>
    void registerComponent()
    {
        const char* typeName = typeid(T).name();

		if (_componentTypeID.find(typeName) != std::end(_componentTypeID))
			return;

        _componentTypeID.insert({typeName, _nextComponentTypeID});
        _componentArrays.insert({typeName, std::make_shared<ComponentArray<T>>()});

        ++_nextComponentTypeID;
    }

    template<typename T>
    std::shared_ptr<ComponentArray<T>> getComponentArray()
    {
        const char* typeName = typeid(T).name();

        if (_componentTypeID.find(typeName) == std::end(_componentTypeID))
            registerComponent<T>();

        return std::static_pointer_cast<ComponentArray<T>>(_componentArrays[typeName]);
    }

private:

	static ComponentManager* instance;

    std::unordered_map<const char*, ComponentTypeID> _componentTypeID;
    std::unordered_map<const char*, std::shared_ptr<IComponentArray>> _componentArrays;
    ComponentTypeID _nextComponentTypeID;

friend class Entity;
};