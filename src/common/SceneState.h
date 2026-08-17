#pragma once

#include <optional>

#include <juce_data_structures/juce_data_structures.h>

#include "SceneModel.h"

namespace clubcraft::scene_state
{

inline constexpr const char* kDynamicSceneTreeType = "DynamicScene";

/** Creates the custom, control-side DynamicScene subtree used by schema 7+. */
[[nodiscard]] juce::ValueTree toValueTree(const DynamicScene& scene);

/** Returns nullopt only when the expected DynamicScene tree is absent or invalid. */
[[nodiscard]] std::optional<DynamicScene> fromValueTree(const juce::ValueTree& tree);

} // namespace clubcraft::scene_state
