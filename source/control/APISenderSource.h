#pragma once
#include "Runtime.h"

namespace control {
// Text limits bound parsing and a whole command batch below the transport queue limit.
inline std::vector<Json> ParseAPICommands(const std::string& text) {
    if (text.size() > 64 * 1024) throw std::runtime_error("API command text exceeds 64 KiB");
    if (text.find_first_not_of(" \t\r\n") == std::string::npos) return {};
    const auto json = Json::parse(text);
    std::vector<Json> commands;
    if (json.is_object()) commands.push_back(json);
    else if (json.is_array()) commands = json.get<std::vector<Json>>();
    else throw std::runtime_error("API commands must be an object or array of objects");
    if (commands.size() > 256) throw std::runtime_error("API command batch exceeds 256 commands");
    for (auto& command : commands) {
        if (!command.is_object() || !command.contains("action") || !command["action"].is_string() ||
            command["action"].get_ref<const std::string&>().empty())
            throw std::runtime_error("each API command needs a nonempty action string");
        // Resolume's WebSocket post envelope expects the HTTP body as text.
        // Preserve explicit strings and all other actions/fields verbatim.
        if (command["action"] == "post" && command.contains("body") &&
            (command["body"].is_object() || command["body"].is_array()))
            command["body"] = command["body"].dump();
    }
    return commands;
}

class APISenderSource final : public ControlSource {
public:
    enum Parameter : unsigned { Enabled, ActiveCommand, InactiveCommand, Identity, Count };
    APISenderSource() { SetKey(NewInstanceKey()); }
    Evaluation Evaluate(const Json& state) override {
        const CompositionIndex index(state);
        return EvaluateIndexed(state, index);
    }
    Evaluation EvaluateIndexed(const Json&, const CompositionIndex& index) override {
        std::lock_guard<std::mutex> lock(mutex);
        Evaluation result;
        result.revision = revision.load();
        result.evidence["plugin"] = "APISender";
        bool active = false;
        if (alive && enabled) {
            const auto resolved = index.Resolve(Key());
            if (!resolved.clip) { result.status = "unresolved_or_duplicate_key"; return result; }
            active = IsActive(*resolved.clip) && resolved.effect->contains("bypassed") &&
                !resolved.effect->at("bypassed").value("value", true);
        }
        result.commandActive = active;
        result.status = active ? "active" : "inactive";
        result.commands = active ? activeCommands : inactiveCommands;
        return result;
    }
    std::uint64_t Revision() const override { return revision.load(); }
    bool SetEnabled(float value) {
        if (value != 0 && value != 1) return false;
        std::lock_guard<std::mutex> lock(mutex);
        enabled = value != 0; ++revision; return true;
    }
    float GetEnabled() { std::lock_guard<std::mutex> lock(mutex); return enabled ? 1.f : 0.f; }
    bool SetText(unsigned id, const std::string& text) {
        if (id == Identity) {
            std::lock_guard<std::mutex> lock(mutex);
            if (!text.empty() && text != Key()) { SetKey(text); ++revision; }
            return true;
        }
        if (id != ActiveCommand && id != InactiveCommand) return false;
        auto commands = ParseAPICommands(text); // Reject the entire edit before changing saved settings.
        std::lock_guard<std::mutex> lock(mutex);
        (id == ActiveCommand ? activeText : inactiveText) = text;
        (id == ActiveCommand ? activeCommands : inactiveCommands) = std::move(commands);
        ++revision;
        return true;
    }
    std::string GetText(unsigned id) {
        std::lock_guard<std::mutex> lock(mutex);
        if (id == Identity) return Key();
        return id == ActiveCommand ? activeText : id == InactiveCommand ? inactiveText : std::string();
    }
    void Close() { std::lock_guard<std::mutex> lock(mutex); alive = false; ++revision; }
private:
    std::mutex mutex;
    std::atomic<std::uint64_t> revision{0};
    bool enabled = true, alive = true;
    std::string activeText, inactiveText;
    std::vector<Json> activeCommands, inactiveCommands;
};
}
