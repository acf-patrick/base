#include "event.h"
#include "input.h"
#include <map>
#include <cstdlib>
#include <ecs/ecs.h>
#include <application/application.h>

EventManagerType::~EventManagerType()
{
    while(!events.empty())
    {
        delete events.front();
        events.pop();
    }
    bind.clear();
    listners.clear();
}

void EventManagerType::handle()
{
    SDLEvents();
    while (!events.empty())
    {
        Event& event = events.front();
        auto tag = event->get<Component::tag>().content;

        for (auto l : listners)
            if (l->enabled)
            {
                auto& c = l->callbacks;
                if (c.find(tag) != c.end())
                    c[tag](*event);
            }

        delete event;
        events.pop();
        bind.erase(tag);
    }
}

void EventManagerType::SDLEvents()
{
    SDL_Event event;
    Entity& e = *static_cast<Entity*>(nullptr);
    while (SDL_PollEvent(&event))
        switch (event.type)
        {
        case SDL_QUIT:
            emit(Input.QUIT);
            break;
        case SDL_KEYDOWN:
            emit(Input.KEY_DOWN);
            Input.keys[event.key.keysym.scancode] = true;
            break;
        case SDL_KEYUP:
            emit(Input.KEY_UP);
            Input.keys[event.key.keysym.scancode] = false;
            break;
        case SDL_MOUSEBUTTONDOWN:
            emit(Input.MOUSE_BUTTON_DOWN).attachIf<SDL_MouseButtonEvent>(event.button);
            break;
        case SDL_MOUSEBUTTONUP:
            emit(Input.MOUSE_BUTTON_UP).attachIf<SDL_MouseButtonEvent>(event.button);
            break;
        case SDL_MOUSEMOTION:
            emit(Input.MOUSE_MOTION).attachIf<SDL_MouseMotionEvent>(event.motion);
            break;
        case SDL_MOUSEWHEEL:
            emit(Input.MOUSE_WHEEL).attachIf<SDL_MouseWheelEvent>(event.wheel);
            break;
        default : ;
        }
}

Entity& EventManagerType::emit(const std::string& event_name)
{
    if (bind.find(event_name) != bind.end())
        return *bind[event_name];
        
    Event event = new Entity;
    event->attach<Component::tag>(event_name);

    bind[event_name] = event;
    events.push(event);

    return *event;
}

void EventManagerType::newListner(EventListner* listner)
{
    listners.push_back(listner);
}

void EventManagerType::listnerDestroyed(EventListner* listner)
{
    listners.erase(std::find(listners.begin(), listners.end(), listner));
}