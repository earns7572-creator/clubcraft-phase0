#include "SceneState.h"

#include <algorithm>

namespace clubcraft::scene_state
{

namespace
{
const juce::Identifier kSpeakerTreeType { "Speaker" };
const juce::Identifier kRouteTreeType { "Route" };

[[nodiscard]] float readFloat(const juce::ValueTree& tree, const char* property, float fallback)
{
    return static_cast<float>(tree.getProperty(property, fallback));
}

[[nodiscard]] bool readBool(const juce::ValueTree& tree, const char* property, bool fallback)
{
    return static_cast<bool>(tree.getProperty(property, fallback));
}

[[nodiscard]] int readInt(const juce::ValueTree& tree, const char* property, int fallback)
{
    return static_cast<int>(tree.getProperty(property, fallback));
}
}

juce::ValueTree toValueTree(const DynamicScene& scene)
{
    juce::ValueTree tree { kDynamicSceneTreeType };
    tree.setProperty("listenerX", scene.listener.x, nullptr);
    tree.setProperty("listenerY", scene.listener.y, nullptr);
    tree.setProperty("masterLevelDb", scene.masterLevelDb, nullptr);
    tree.setProperty("genericResponseTone", scene.genericResponseTone, nullptr);
    tree.setProperty("legacyDefaultRouting", scene.legacyDefaultRouting, nullptr);
    tree.setProperty("legacyRouteGain", scene.legacyRouteGain, nullptr);

    for (const auto& speaker : scene.speakers)
    {
        juce::ValueTree speakerTree { kSpeakerTreeType };
        speakerTree.setProperty("stableId", juce::String(speaker.stableId), nullptr);
        speakerTree.setProperty("name", juce::String(speaker.name), nullptr);
        speakerTree.setProperty("type", speakerTypeToParameterIndex(speaker.type), nullptr);
        speakerTree.setProperty("x", speaker.position.x, nullptr);
        speakerTree.setProperty("y", speaker.position.y, nullptr);
        speakerTree.setProperty("levelDb", speaker.levelDb, nullptr);
        speakerTree.setProperty("enabled", speaker.enabled, nullptr);
        tree.appendChild(speakerTree, nullptr);
    }

    for (const auto& route : scene.routes)
    {
        juce::ValueTree routeTree { kRouteTreeType };
        routeTree.setProperty("stableId", juce::String(route.stableId), nullptr);
        routeTree.setProperty("sourceId", juce::String(route.sourceId), nullptr);
        routeTree.setProperty("speakerStableId", juce::String(route.speakerStableId), nullptr);
        routeTree.setProperty("mode", static_cast<int>(route.mode), nullptr);
        routeTree.setProperty("inputMode", static_cast<int>(route.inputMode), nullptr);
        routeTree.setProperty("bandLowHz", route.bandLowHz, nullptr);
        routeTree.setProperty("bandHighHz", route.bandHighHz, nullptr);
        routeTree.setProperty("gainLinear", route.gainLinear, nullptr);
        routeTree.setProperty("enabled", route.enabled, nullptr);
        tree.appendChild(routeTree, nullptr);
    }

    return tree;
}

std::optional<DynamicScene> fromValueTree(const juce::ValueTree& tree)
{
    if (!tree.isValid() || !tree.hasType(kDynamicSceneTreeType))
        return std::nullopt;

    DynamicScene scene;
    scene.listener = {
        readFloat(tree, "listenerX", 0.0f),
        readFloat(tree, "listenerY", 0.0f),
    };
    scene.masterLevelDb = readFloat(tree, "masterLevelDb", 0.0f);
    scene.genericResponseTone = readFloat(tree, "genericResponseTone", 1.0f);
    scene.legacyDefaultRouting = readBool(tree, "legacyDefaultRouting", false);
    scene.legacyRouteGain = readFloat(tree, "legacyRouteGain", 0.25f);

    for (const auto& child : tree)
    {
        if (child.hasType(kSpeakerTreeType))
        {
            if (scene.speakers.size() >= kMaxSpeakers)
                return std::nullopt;

            scene.speakers.push_back({
                .stableId = child.getProperty("stableId").toString().toStdString(),
                .name = child.getProperty("name").toString().toStdString(),
                .type = speakerTypeFromParameterIndex(readInt(child, "type", speakerTypeToParameterIndex(SpeakerType::fullRange))),
                .position = {
                    readFloat(child, "x", 0.0f),
                    readFloat(child, "y", 0.0f),
                },
                .levelDb = readFloat(child, "levelDb", 0.0f),
                .enabled = readBool(child, "enabled", true),
            });
        }
        else if (child.hasType(kRouteTreeType))
        {
            if (scene.routes.size() >= kMaxRoutesGlobal)
                return std::nullopt;

            scene.routes.push_back({
                .stableId = child.getProperty("stableId").toString().toStdString(),
                .sourceId = child.getProperty("sourceId").toString().toStdString(),
                .speakerStableId = child.getProperty("speakerStableId").toString().toStdString(),
                .mode = static_cast<RouteMode>(readInt(child, "mode", static_cast<int>(RouteMode::full))),
                .inputMode = static_cast<InputChannelMode>(readInt(child, "inputMode", static_cast<int>(InputChannelMode::sumMono))),
                .bandLowHz = readFloat(child, "bandLowHz", 20.0f),
                .bandHighHz = readFloat(child, "bandHighHz", 20000.0f),
                .gainLinear = readFloat(child, "gainLinear", 1.0f),
                .enabled = readBool(child, "enabled", true),
            });
        }
    }

    return scene;
}

} // namespace clubcraft::scene_state
