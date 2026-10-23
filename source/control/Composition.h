#pragma once
#include "StateReconciler.h"
#include <stdexcept>

namespace control {
inline bool IsComposition(const Json& message) {
    return message.is_object() && message.contains("layers") && message["layers"].is_array();
}
inline bool IsActive(const Json& clip) {
    if (!clip.contains("connected") || !clip["connected"].is_object()) return false;
    const auto value = clip["connected"].value("value", std::string());
    return value == "Connected" || value == "Connected & previewing";
}
inline std::string ParameterPath(const Json& parameter) {
    if (!parameter.contains("id") || !parameter["id"].is_number_integer() ||
        parameter["id"].get<std::int64_t>() <= 0)
        throw std::runtime_error("missing parameter id");
    return "/parameter/by-id/" + std::to_string(parameter["id"].get<std::int64_t>());
}
// Count the token everywhere, including non-Clip placements. Never choose the first match.
inline unsigned TokenOccurrences(const Json& node, const std::string& key) {
    unsigned count = 0;
    if (node.is_object()) {
        if (node.contains("Instance Key") && node["Instance Key"].is_object() &&
            node["Instance Key"].value("value", std::string()) == key) ++count;
        // Arena repeats the playing Clip under layer.active_clip. It is a view of
        // the same entity, not a second FFGL placement; resolve only canonical clips.
        for (const auto& child : node.items())
            if (child.key() != "active_clip") count += TokenOccurrences(child.value(), key);
    } else if (node.is_array()) {
        for (const auto& child : node) count += TokenOccurrences(child, key);
    }
    return count;
}
struct ResolvedClip { const Json* clip = nullptr; const Json* effect = nullptr; };
inline ResolvedClip ResolveClip(const Json& composition, const std::string& key) {
    if (key.empty() || !IsComposition(composition) || TokenOccurrences(composition, key) != 1) return {};
    for (const auto& layer : composition["layers"]) {
        if (!layer.contains("clips") || !layer["clips"].is_array()) continue;
        for (const auto& clip : layer["clips"]) {
            if (!clip.contains("video") || !clip["video"].is_object()) continue;
            const auto& video = clip["video"];
            if (!video.contains("effects") || !video["effects"].is_array()) continue;
            for (const auto& effect : video["effects"]) {
                if (!effect.contains("params") || !effect["params"].is_object()) continue;
                const auto& params = effect["params"];
                if (params.contains("Instance Key") && params["Instance Key"].value("value", std::string()) == key &&
                    clip.contains("id") && clip["id"].is_number_integer()) return { &clip, &effect };
            }
        }
    }
    return {};
}
}
