//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Debug.cc
//| Author(s)   : Niklas Een
//| Module      : RallyVroom
//| Description : 
//| 
//| (C) Copyright 2014, Niklas Een
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "Debug.hh"


namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


struct SDL_EventName {
    SDL_EventType type;
    cchar* name;
};


struct SDL_WindowEventName {
    SDL_WindowEventID type;
    cchar* name;
};


static const SDL_EventName sdl_event_type[] = {
    { SDL_FIRSTEVENT,               "FIRSTEVENT" },
    { SDL_QUIT,                     "QUIT" },
    { SDL_APP_TERMINATING,          "APP_TERMINATING" },
    { SDL_APP_LOWMEMORY,            "APP_LOWMEMORY" },
    { SDL_APP_WILLENTERBACKGROUND,  "APP_WILLENTERBACKGROUND" },
    { SDL_APP_DIDENTERBACKGROUND,   "APP_DIDENTERBACKGROUND" },
    { SDL_APP_WILLENTERFOREGROUND,  "APP_WILLENTERFOREGROUND" },
    { SDL_APP_DIDENTERFOREGROUND,   "APP_DIDENTERFOREGROUND" },
    { SDL_WINDOWEVENT,              "WINDOWEVENT" },
    { SDL_SYSWMEVENT,               "SYSWMEVENT" },
    { SDL_KEYDOWN,                  "KEYDOWN" },
    { SDL_KEYUP,                    "KEYUP" },
    { SDL_TEXTEDITING,              "TEXTEDITING" },
    { SDL_TEXTINPUT,                "TEXTINPUT" },
    { SDL_MOUSEMOTION,              "MOUSEMOTION" },
    { SDL_MOUSEBUTTONDOWN,          "MOUSEBUTTONDOWN" },
    { SDL_MOUSEBUTTONUP,            "MOUSEBUTTONUP" },
    { SDL_MOUSEWHEEL,               "MOUSEWHEEL" },
    { SDL_JOYAXISMOTION,            "JOYAXISMOTION" },
    { SDL_JOYBALLMOTION,            "JOYBALLMOTION" },
    { SDL_JOYHATMOTION,             "JOYHATMOTION" },
    { SDL_JOYBUTTONDOWN,            "JOYBUTTONDOWN" },
    { SDL_JOYBUTTONUP,              "JOYBUTTONUP" },
    { SDL_JOYDEVICEADDED,           "JOYDEVICEADDED" },
    { SDL_JOYDEVICEREMOVED,         "JOYDEVICEREMOVED" },
    { SDL_CONTROLLERAXISMOTION,     "CONTROLLERAXISMOTION" },
    { SDL_CONTROLLERBUTTONDOWN,     "CONTROLLERBUTTONDOWN" },
    { SDL_CONTROLLERBUTTONUP,       "CONTROLLERBUTTONUP" },
    { SDL_CONTROLLERDEVICEADDED,    "CONTROLLERDEVICEADDED" },
    { SDL_CONTROLLERDEVICEREMOVED,  "CONTROLLERDEVICEREMOVED" },
    { SDL_CONTROLLERDEVICEREMAPPED, "CONTROLLERDEVICEREMAPPED" },
    { SDL_FINGERDOWN,               "FINGERDOWN" },
    { SDL_FINGERUP,                 "FINGERUP" },
    { SDL_FINGERMOTION,             "FINGERMOTION" },
    { SDL_DOLLARGESTURE,            "DOLLARGESTURE" },
    { SDL_DOLLARRECORD,             "DOLLARRECORD" },
    { SDL_MULTIGESTURE,             "MULTIGESTURE" },
    { SDL_CLIPBOARDUPDATE,          "CLIPBOARDUPDATE" },
    { SDL_DROPFILE,                 "DROPFILE" },
    { SDL_RENDER_TARGETS_RESET,     "RENDER_TARGETS_RESET" },
    { SDL_USEREVENT,                "USEREVENT" },
    { SDL_LASTEVENT,                "LASTEVENT" },
};


static const SDL_WindowEventName sdl_window_event_type[] = {
    { SDL_WINDOWEVENT_NONE,           "NONE" },
    { SDL_WINDOWEVENT_SHOWN,          "SHOWN" },
    { SDL_WINDOWEVENT_HIDDEN,         "HIDDEN" },
    { SDL_WINDOWEVENT_EXPOSED,        "EXPOSED" },
    { SDL_WINDOWEVENT_MOVED,          "MOVED" },
    { SDL_WINDOWEVENT_RESIZED,        "RESIZED" },
    { SDL_WINDOWEVENT_SIZE_CHANGED,   "SIZE_CHANGED" },
    { SDL_WINDOWEVENT_MINIMIZED,      "MINIMIZED" },
    { SDL_WINDOWEVENT_MAXIMIZED,      "MAXIMIZED" },
    { SDL_WINDOWEVENT_RESTORED,       "RESTORED" },
    { SDL_WINDOWEVENT_ENTER,          "ENTER" },
    { SDL_WINDOWEVENT_LEAVE,          "LEAVE" },
    { SDL_WINDOWEVENT_FOCUS_GAINED,   "FOCUS_GAINED" },
    { SDL_WINDOWEVENT_FOCUS_LOST,     "FOCUS_LOST" },
    { SDL_WINDOWEVENT_CLOSE,          "CLOSE" },
};


void writeSdlEvent(Out& out, const SDL_Event& ev)
{
    if (ev.type == SDL_WINDOWEVENT){
        for (uint j = 0; j < elemsof(sdl_window_event_type); j++){
            if (sdl_window_event_type[j].type == ev.window.event){
                FWrite(out) "SdlWinEvent{%_ : %_ %_}", sdl_window_event_type[j].name, ev.window.data1, ev.window.data2;
                return;
            }
        }
        FWrite(out) "SdlWinEvent{event=%_; data1=%_; data2=%_}", ev.window.event, ev.window.data1, ev.window.data2;
        return;
    }

    for (uint i = 0; i < elemsof(sdl_event_type); i++){
        if (i+1 < elemsof(sdl_event_type))
            assert(sdl_event_type[i].type < sdl_event_type[i+1].type);

        if ((uint)ev.type >= (uint)sdl_event_type[i].type && (i+1 == elemsof(sdl_event_type) || (uint)ev.type < (uint)sdl_event_type[i+1].type)){
            if ((uint)ev.type == (uint)sdl_event_type[i].type)
                FWrite(out) "SdlEvent{%s}", sdl_event_type[i].name;
            else
                FWrite(out) "SdlEvent{%s + %_}", sdl_event_type[i].name, (uint)ev.type - (uint)sdl_event_type[i].type;

            return;
        }
    }
    assert(false);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
