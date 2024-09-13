#include "event.h"

#include <cstdlib>
#include <map>

#include "../application/application.h"
#include "../ecs/ecs.h"
#include "input.h"

EventManager::~EventManager() {
    while (!events.empty()) {
        delete events.front();
        events.pop();
    }
    bind.clear();
    listners.clear();
}

void EventManager::handle() {
    SDLEvents();
    while (!events.empty()) {
        Event& event = events.front();
        auto tag = event->get<Component::tag>().content;

        for (int i = 0; i < (int)listners.size(); ++i) {
            auto& listener = *listners[i];
            if (listener.enabled) {
                auto& callbacks = listener.callbacks;
                if (callbacks.find(tag) != callbacks.end()) {
                    auto& callback = callbacks[tag];
                    if (callback.withParameter) {
                        auto& function = *callback.function;
                        function(*event);
                    } else {
                        auto& function = *callback.noParamFunction;
                        function();
                    }
                }
            }
        }

        delete event;
        events.pop();
        bind.erase(tag);
    }
}

void EventManager::SDLEvents() {
    auto& input = Input::Get();
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                _emit(input.QUIT);
                break;
            case SDL_KEYDOWN:
                _emit(input.KEY_DOWN).attachIf<SDL_KeyboardEvent>(event.key);
                input._keys[event.key.keysym.scancode] = true;
                break;
            case SDL_KEYUP:
                _emit(input.KEY_UP).attachIf<SDL_KeyboardEvent>(event.key);
                input._keys[event.key.keysym.scancode] = false;
                break;
            case SDL_MOUSEBUTTONDOWN:
                _emit(input.MOUSE_BUTTON_DOWN)
                    .attachIf<SDL_MouseButtonEvent>(event.button);
                break;
            case SDL_MOUSEBUTTONUP:
                _emit(input.MOUSE_BUTTON_UP)
                    .attachIf<SDL_MouseButtonEvent>(event.button);
                break;
            case SDL_MOUSEMOTION:
                _emit(input.MOUSE_MOTION)
                    .attachIf<SDL_MouseMotionEvent>(event.motion);
                break;
            case SDL_MOUSEWHEEL:
                _emit(input.MOUSE_WHEEL)
                    .attachIf<SDL_MouseWheelEvent>(event.wheel);
                break;
            default:;
        }
    }
}

Entity& EventManager::emit(const std::string& event_name) {
    auto& input = Input::Get();
    std::vector<std::string> reserved = {input.QUIT,
                                         input.KEY_DOWN,
                                         input.KEY_UP,
                                         input.MOUSE_BUTTON_DOWN,
                                         input.MOUSE_BUTTON_UP,
                                         input.MOUSE_WHEEL,
                                         input.MOUSE_MOTION,
                                         input.SCENE_LOADED,
                                         input.SCENE_CHANGED};

    auto invalidEvent = std::find(reserved.begin(), reserved.end(),
                                  event_name) != reserved.end();
    auto message = "You can not emit event : " + event_name;
    assert(invalidEvent && message.c_str());

    return _emit(event_name);
}

Entity& EventManager::_emit(const std::string& event_name) {
    if (bind.find(event_name) != bind.end()) return *bind[event_name];

    Event event = new Entity;
    event->attach<Component::tag>(event_name);

    bind[event_name] = event;
    events.push(event);

    return *event;
}

void EventManager::newListner(EventListner* listner) {
    listners.push_back(listner);
}

void EventManager::listnerDestroyed(EventListner* listner) {
    listners.erase(std::remove(listners.begin(), listners.end(), listner));
}

// static
std::shared_ptr<EventManager> EventManager::Get() { return createInstance(); }