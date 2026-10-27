#pragma once
#include "StateReconciler.h"
#include <stdexcept>

namespace control {
inline Json ParseStateMessage(const std::string& raw) {
    bool groups = false;
    return Json::parse(raw, [&](int depth, Json::parse_event_t event, Json& value) {
        if (event != Json::parse_event_t::key) return true;
        if (depth == 1) groups = value == "layergroups";
        // These are duplicate views, not placements. Keep Group-level effects.
        if (groups && depth == 3 && value == "layers") return false;
        if (!groups && depth == 3 && value == "active_clip") return false;
        return true;
    });
}
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
// Built once per snapshot, shared by every Source evaluation. References never
// outlive the immutable snapshot. Count non-Clip placements as well.
class CompositionIndex {
public:
    explicit CompositionIndex(const Json& composition) {
        for (const auto& child : composition.items()) {
            if (child.key() != "layergroups") Count(child.value());
            else if (child.value().is_array()) for (const auto& group : child.value())
                if (group.is_object()) for (const auto& field : group.items())
                    if (field.key() != "layers") Count(field.value());
        }
        if (!IsComposition(composition)) return;
        for (const auto& layer : composition["layers"]) {
            if (!layer.contains("clips") || !layer["clips"].is_array()) continue;
            for (const auto& clip : layer["clips"]) {
                if (!clip.contains("id") || !clip["id"].is_number_integer() ||
                    !clip.contains("video") || !clip["video"].is_object()) continue;
                const auto& video = clip["video"];
                if (!video.contains("effects") || !video["effects"].is_array()) continue;
                for (const auto& effect : video["effects"]) {
                    if (!effect.contains("params") || !effect["params"].is_object()) continue;
                    const auto& params = effect["params"];
                    if (!params.contains("Instance Key")) continue;
                    const auto& token = params["Instance Key"];
                    if (token.is_object() && token.contains("value") && token["value"].is_string())
                        clips[token["value"].get<std::string>()] = {&clip, &effect};
                }
            }
        }
    }
    ResolvedClip Resolve(const std::string& key) const {
        const auto count = counts.find(key); const auto clip = clips.find(key);
        return !key.empty() && count != counts.end() && count->second == 1 && clip != clips.end()
            ? clip->second : ResolvedClip{};
    }
private:
    void Count(const Json& node) {
        if (node.is_object()) {
            const auto token = node.find("Instance Key");
            if (token != node.end() && token->is_object() && token->contains("value") && (*token)["value"].is_string())
                ++counts[(*token)["value"].get<std::string>()];
            for (const auto& child : node.items()) if (child.key() != "active_clip") Count(child.value());
        } else if (node.is_array()) for (const auto& child : node) Count(child);
    }
    std::map<std::string, unsigned> counts;
    std::map<std::string, ResolvedClip> clips;
};
// Arena 7.27.1 includes every Layer in composition.layers, including group
// members. layergroups[].layers repeats those same objects, not placements.
inline unsigned CanonicalTokenOccurrences(const Json& composition, const std::string& key) {
    // Only canonical layers are placements. Group member snapshots are views,
    // and may contain older dynamic values. Group-level effects still count.
    unsigned count = 0;
    for (const auto& child : composition.items()) {
        if (child.key() != "layergroups") count += TokenOccurrences(child.value(), key);
        else if (child.value().is_array())
            for (const auto& group : child.value())
                if (group.is_object()) for (const auto& field : group.items())
                    if (field.key() != "layers") count += TokenOccurrences(field.value(), key);
    }
    return count;
}
inline ResolvedClip ResolveClip(const Json& composition, const std::string& key) {
    if (key.empty() || !IsComposition(composition) || CanonicalTokenOccurrences(composition, key) != 1) return {};
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
