#pragma once
#include "Composition.h"
#include <cmath>

namespace control {
using Settings = std::vector<float>;
inline unsigned RoleIndex(const Json& d, const std::string& role) {
    for (const auto& p : d.at("parameters")) if (p.value("role", "") == role) return p.at("id").get<unsigned>();
    throw std::runtime_error("missing role");
}
inline Settings Defaults(const Json& d) {
    Settings result;
    for (const auto& p : d.at("parameters")) result.push_back(p.value("default", 0.0f));
    return result;
}
inline bool Valid(const Json& d, unsigned index, float v) {
    if (!std::isfinite(v) || index >= d.at("parameters").size()) return false;
    const auto& p = d["parameters"][index];
    const auto type = p.at("type").get<std::string>();
    if (type == "text" || p.value("role", "") == "reserved") return false;
    if (type == "boolean") return v == 0 || v == 1;
    if (type == "choice") return v >= 0 && v < p.at("options").size() && std::floor(v) == v;
    return v >= p.at("min").get<float>() && v <= p.at("max").get<float>() && (type != "integer" || std::floor(v) == v);
}
inline Intent Choice(const std::string& owner, const Json& p, const std::string& desired) {
    if (p.value("valuetype", "") != "ParamChoice" || !p.contains("options") || !p["options"].is_array() ||
        !p.contains("value") || !p["value"].is_string() ||
        std::count(p["options"].begin(), p["options"].end(), Json(desired)) != 1 ||
        std::count(p["options"].begin(), p["options"].end(), p["value"]) != 1)
        throw std::runtime_error("unsupported choice metadata/value");
    return {owner, ParameterPath(p), p["value"], desired};
}
inline const Json& Property(const Json& root, const std::string& property) {
    const Json* value = &root;
    std::size_t begin = 0;
    while (begin < property.size()) {
        auto end = property.find('.', begin);
        value = &value->at(property.substr(begin, end == std::string::npos ? end : end - begin));
        if (end == std::string::npos) return *value;
        begin = end + 1;
    }
    throw std::runtime_error("empty property");
}
inline std::vector<Intent> BuildIntents(const Json& d, const Json& composition, const std::string& key,
                                      const Settings& settings, std::string& status) {
    const auto& params = d.at("parameters");
    if (settings.size() != params.size()) throw std::runtime_error("settings size mismatch");
    for (unsigned i = 0; i < settings.size(); ++i)
        if (params[i]["type"] != "text" && params[i].value("role", "") != "reserved" && !Valid(d,i,settings[i]))
            throw std::runtime_error("invalid setting");
    if (!settings[RoleIndex(d,"enabled")]) { status = "disabled"; return {}; }
    auto resolved = ResolveClip(composition,key);
    if (!resolved.clip) { status = "unresolved_or_duplicate_key"; return {}; }
    if (!IsActive(*resolved.clip)) { status = "inactive"; return {}; }
    if (!resolved.effect->contains("bypassed") || resolved.effect->at("bypassed").value("value",true)) { status = "bypassed_or_unknown"; return {}; }
    const auto index = static_cast<std::size_t>(settings[RoleIndex(d,"target")] - 1);
    if (index >= composition.at("layers").size()) throw std::runtime_error("target layer missing");
    if (composition.contains("layergroups") && !composition["layergroups"].empty()) throw std::runtime_error("group numbering not validated");
    const auto& layer = composition["layers"][index];
    std::vector<Intent> intents;
    for (const auto& p : params) {
        if (!p.contains("property")) continue;
        const auto& actual = Property(layer,p.at("property"));
        const float desired = settings.at(p.at("id").get<unsigned>());
        if (p["type"] == "choice") {
            intents.push_back(Choice(key,actual,p.at("options").at(static_cast<unsigned>(desired))));
        } else if (p["type"] == "boolean") {
            if (actual.value("valuetype","") != "ParamBoolean" || !actual.at("value").is_boolean()) throw std::runtime_error("invalid boolean metadata");
            intents.push_back({key,ParameterPath(actual),actual["value"],desired != 0});
        } else {
            if (actual.value("valuetype","") != "ParamRange" || !actual.at("value").is_number() ||
                !actual.at("min").is_number() || !actual.at("max").is_number() ||
                desired < actual.at("min").get<double>() || desired > actual.at("max").get<double>())
                throw std::runtime_error("invalid numeric metadata/range");
            intents.push_back({key,ParameterPath(actual),actual["value"],desired,p.value("tolerance",0.0)});
        }
    }
    status = "active";
    return intents;
}
}
