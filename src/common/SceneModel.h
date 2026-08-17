#pragma once

#include <string>
#include <vector>

#include "SceneTypes.h"
#include "SpeakerType.h"

namespace clubcraft
{

/**
    Control-side, persistable description of a Club. This type may use strings
    and vectors because it is never read or modified by the audio callback.
*/
struct SpeakerConfig
{
    std::string stableId;
    std::string name;
    SpeakerType type = SpeakerType::fullRange;
    PlanarPosition position;
    float levelDb = 0.0f;
    bool enabled = true;
};

/**
    A saved route uses stable IDs. SceneCompiler resolves them to numeric slots
    and generations before a SOURCE instance receives the route on audio thread.
*/
struct RouteConfig
{
    std::string stableId;
    std::string sourceId;
    std::string speakerStableId;
    RouteMode mode = RouteMode::full;
    InputChannelMode inputMode = InputChannelMode::sumMono;
    float bandLowHz = 20.0f;   // Reserved in 0.6.0; BAND DSP is not enabled yet.
    float bandHighHz = 20000.0f;
    float gainLinear = 1.0f;
    bool enabled = true;
};

struct DynamicScene
{
    std::vector<SpeakerConfig> speakers;
    std::vector<RouteConfig> routes;
    PlanarPosition listener;
    float masterLevelDb = 0.0f;
    float genericResponseTone = 1.0f;

    // schema <= 6 projects never stored routes. The compiler generates the
    // implicit source-to-first-four-speakers routes at legacyRouteGain.
    bool legacyDefaultRouting = false;
    float legacyRouteGain = 0.25f;
};

} // namespace clubcraft
