#pragma once
#include "Composition.h"
#include "WebSocket.h"
#include "Platform.h"
#include "ServerSettings.h"
#include <atomic>
#include <fstream>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>

namespace control {
struct Evaluation {
    std::vector<Intent> intents;
    std::string status;
    std::uint64_t revision = 0;
    Json evidence = Json::object();
    std::set<std::string> heldProperties;
};
struct ControlSource {
    virtual ~ControlSource() = default;
    virtual Evaluation Evaluate(const Json&) = 0;
    virtual std::uint64_t Revision() const = 0;
    std::string Key() const { std::lock_guard<std::mutex> lock(keyMutex); return key; }
    void SetKey(const std::string& value) { std::lock_guard<std::mutex> lock(keyMutex); key = value; }
protected:
    std::string key;
    mutable std::mutex keyMutex;
};
// Shared per DLL, not per FFGL instance. No host/GL callbacks on the worker.
class Runtime {
public:
    static std::shared_ptr<Runtime> Acquire() {
        static std::mutex creationMutex;
        static std::weak_ptr<Runtime> existing;
        std::lock_guard<std::mutex> lock(creationMutex);
        auto runtime = existing.lock();
        if (!runtime) { runtime = std::shared_ptr<Runtime>(new Runtime); existing = runtime; }
        return runtime;
    }
    void Add(const std::shared_ptr<ControlSource>& source) {
        std::lock_guard<std::mutex> lock(mutex);
        sources.erase(std::remove_if(sources.begin(), sources.end(), [](const auto& p) { return p.expired(); }), sources.end());
        if (sources.size() >= 128) throw std::runtime_error("128 instance limit");
        sources.push_back(source);
    }
    ~Runtime() {
        stop.store(true);
        socket.Cancel();
        if (worker.joinable()) worker.join();
    }
#ifdef RLC_RUNTIME_TESTING
    static std::shared_ptr<Runtime> ForTest(const std::filesystem::path& file) {
        return std::shared_ptr<Runtime>(new Runtime(file));
    }
#endif
private:
    std::filesystem::path HostSettingsFile() const {
#ifdef RLC_RUNTIME_TESTING
        if (!testSettingsFile.empty()) return testSettingsFile;
#endif
        return HostServerSettingsPath();
    }
#ifdef RLC_RUNTIME_TESTING
    explicit Runtime(const std::filesystem::path& file) : testSettingsFile(file), worker([this] { Run(); }) {}
    std::filesystem::path testSettingsFile;
#endif
    Runtime() : worker([this] { Run(); }) {}
    void Log(const Json& entry) {
        Json row = entry;
        row["wall_ms"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        const auto text = row.dump();
        DebugLog("LayerControl " + text + "\n");
        if (log.is_open() && logBytes < 16 * 1024 * 1024) { log << text << '\n'; log.flush(); logBytes += text.size() + 1; }
    }
    std::vector<std::shared_ptr<ControlSource>> Sources() {
        std::lock_guard<std::mutex> lock(mutex);
        std::vector<std::shared_ptr<ControlSource>> result;
        for (auto& weak : sources) if (auto source = weak.lock()) result.push_back(source);
        return result;
    }
    void Confirm(const Json& composition) {
        for (const auto& path : reconciler.PendingPaths()) {
            const auto* current = FindParameter(composition, path);
            if (current && current->contains("value") && reconciler.ReadBack(path, (*current)["value"])) {
                deadlines.erase(path);
                Log({{"event", "readback"}, {"path", path}, {"value", (*current)["value"]}});
            }
        }
    }
    void Expire() {
        const auto now = std::chrono::steady_clock::now();
        for (auto it = deadlines.begin(); it != deadlines.end();) {
            if (now < it->second) { ++it; continue; }
            reconciler.Fail(it->first);
            Log({{"event", "readback_timeout"}, {"path", it->first}});
            it = deadlines.erase(it);
        }
    }
    void Composition(const Json& composition) {
        // Confirm the received state before considering a timeout.
        Confirm(composition);
        Expire();
        std::vector<Intent> all;
        std::map<std::string, std::pair<std::shared_ptr<ControlSource>, std::uint64_t>> revisions;
        std::set<std::string> active, liveOwners;
        for (auto& source : Sources()) {
            const auto owner = source->Key();
            liveOwners.insert(owner);
            auto& previous = ownerPaths[owner];
            try {
                auto evaluation = source->Evaluate(composition);
                Log({{"event", "gate"}, {"owner", owner}, {"status", evaluation.status}, {"evidence", evaluation.evidence}});
                if (source->Revision() != evaluation.revision || evaluation.status == "unresolved_or_duplicate_key") {
                    for (const auto& old : previous) active.insert(old.first);
                    continue;
                }
                revisions[owner] = {source, evaluation.revision};
                std::map<std::string, std::string> next;
                for (const auto& old : previous)
                    if (evaluation.heldProperties.count(old.second)) {
                        next.insert(old); active.insert(old.first);
                    }
                for (auto intent : evaluation.intents) {
                    intent.revision = evaluation.revision;
                    next[intent.path] = intent.property;
                    active.insert(intent.path);
                    all.push_back(intent);
                    Log({{"event", "compare"}, {"owner", owner}, {"path", intent.path}, {"actual", intent.actual}, {"desired", intent.desired}});
                }
                previous = std::move(next);
            } catch (const std::exception& e) {
                // Only this owner's previous targets are unknown; others can restore.
                for (const auto& old : previous) active.insert(old.first);
                Log({{"event", "invalid_state"}, {"owner", owner}, {"reason", e.what()}});
            }
        }
        for (auto it = ownerPaths.begin(); it != ownerPaths.end();)
            if (!liveOwners.count(it->first)) it = ownerPaths.erase(it); else ++it;
        std::vector<Intent> restores;
        for (auto it = baselines.begin(); it != baselines.end();) {
            if (active.count(it->first)) { ++it; continue; }
            const auto* current = FindParameter(composition, it->first);
            // Missing once is not proof of deletion. Keep the restoration record.
            if (!current || !current->contains("value")) { ++it; continue; }
            const auto& actual = (*current)["value"];
            if (!reconciler.Pending(it->first) && Matches(actual, it->second.value, it->second.tolerance)) {
                it = baselines.erase(it); continue;
            }
            restores.push_back({"", it->first, actual, it->second.value, it->second.tolerance});
            ++it;
        }
        auto unchanged = [&](const std::string& path) {
            for (const auto& i : all) if (i.path == path) {
                const auto& r = revisions.at(i.owner);
                if (r.first->Revision() != r.second) return false;
            }
            return true;
        };
        Apply(all, false, unchanged);
        Apply(restores, true);
    }
    void Apply(std::vector<Intent> intents, bool restoring,
               const std::function<bool(const std::string&)>& unchanged = [](const auto&) { return true; }) {
        // A new desired value supersedes an older generation. Responses never
        // become the source of a desired value; only a fresh Composition does.
        intents.erase(std::remove_if(intents.begin(), intents.end(), [&](const auto& i) { return !unchanged(i.path); }), intents.end());
        for (auto& i : intents) {
            const auto* pending = reconciler.Pending(i.path);
            if (pending && pending->desired != i.desired) {
                reconciler.Fail(i.path); deadlines.erase(i.path); i.force = true;
            }
        }
        for (const auto& intent : reconciler.Plan(intents)) {
            if (stop.load()) return;
            if (!unchanged(intent.path)) continue;
            const auto retry = retryAfter.find(intent.path);
            if (retry != retryAfter.end() && retry->second.desired == intent.desired && retry->second.owner == intent.owner &&
                retry->second.revision == intent.revision &&
                std::chrono::steady_clock::now() < retry->second.until) continue;
            // Capture before the first successful queued change; retain through restore readback.
            const bool inserted = !restoring && baselines.emplace(intent.path, Baseline{intent.actual, intent.tolerance}).second;
            if (!Send(intent, restoring ? "restore_sent" : "set_sent") && inserted) baselines.erase(intent.path);
        }
    }
    bool Send(const Intent& intent, const char* event) {
        if (reconciler.TrackedCount() >= StateReconciler::Capacity) { Log({{"event", "pending_limit"}}); return false; }
        const Json request = {{"action", "set"}, {"parameter", intent.path}, {"value", intent.desired}};
        if (!socket.Send(request.dump())) {
            Defer(intent);
            Log({{"event", "send_deferred"}, {"path", intent.path}});
            return false;
        }
        reconciler.Sent(intent);
        deadlines[intent.path] = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        Log({{"event", event}, {"generation", ++generation}, {"owner", intent.owner}, {"path", intent.path}, {"actual", intent.actual}, {"desired", intent.desired}});
        if (!socket.Send(Json({{"action", "get"}, {"parameter", intent.path}}).dump()))
            Log({{"event", "readback_request_deferred"}, {"path", intent.path}});
        return true;
    }
    void Defer(const Intent& intent) {
        // Bounded cooldown history, independent of pending request capacity.
        if (retryAfter.size() >= StateReconciler::Capacity && !retryAfter.count(intent.path)) retryAfter.erase(retryAfter.begin());
        retryAfter[intent.path] = {intent.desired, intent.owner, intent.revision, std::chrono::steady_clock::now() + std::chrono::seconds(5)};
    }
    static const Json* FindParameter(const Json& node, const std::string& path) {
        static const std::string prefix = "/parameter/by-id/";
        if (path.compare(0, prefix.size(), prefix) != 0) return nullptr;
        const auto id = std::stoll(path.substr(prefix.size()));
        return node.contains("layers") ? FindById(node["layers"], id) : nullptr;
    }
    static const Json* FindById(const Json& node, long long id) {
        if (node.is_object()) {
            const auto found = node.find("id");
            if (found != node.end() && found->is_number_integer() && found->get<long long>() == id && node.contains("valuetype")) return &node;
            for (const auto& child : node.items())
                if (child.key() != "clips" && child.key() != "active_clip")
                    if (const auto* r = FindById(child.value(), id)) return r;
        } else if (node.is_array()) {
            for (const auto& child : node) if (const auto* r = FindById(child, id)) return r;
        }
        return nullptr;
    }
    void Run() noexcept {
        try {
            try {
            const auto path = LogPathFromEnvironment();
            if (!path.empty()) {
                log.open(path, std::ios::app);
                if (!log) { log.clear(); DebugLog("LayerControl: cannot open RLC_WS_LOG; continuing without file log\n"); }
            }
            } catch (const std::exception&) { DebugLog("LayerControl: invalid log path; continuing without file log\n"); }
            auto file = HostSettingsFile();
            Log({{"event", "server_settings"}, {"path", file.u8string()}});
            const auto settings = ReadServerSettings(file);
            if (!settings.enabled) { Log({{"event", "disabled"}, {"reason", "Resolume Webserver disabled"}}); return; }
            const auto port = settings.port;
            if (stop.load()) return;
            socket.Connect(static_cast<unsigned short>(port), settings.host);
            Log({{"event", "connected"}, {"host", settings.host}, {"port", port}});
            std::string raw;
            while (!stop.load() && socket.Receive(raw)) {
                try {
                    const auto message = Json::parse(raw);
                    if (!message.is_object()) continue;
                    if (IsComposition(message)) { Composition(message); continue; }
                    const auto type = message.value("type", std::string());
                    // Resolume replies to by-id requests with a composition path plus id.
                    // Match the stable parameter id, not that presentation path.
                    const auto path = message.contains("id") && message["id"].is_number_integer() &&
                        message["id"].get<long long>() > 0
                        ? ParameterPath(message) : message.value("path", std::string());
                    if (type == "parameter_set") Log({{"event", "server_response"}, {"message", message}});
                    if ((type == "parameter_get" || type == "parameter_update") && message.contains("value") &&
                        reconciler.ReadBack(path, message["value"])) {
                        deadlines.erase(path);
                        Log({{"event", "readback"}, {"path", path}, {"value", message["value"]}});
                    }
                    // Individual updates (including position) never start reconciliation.
                    if (message.contains("error") && !message["error"].is_null()) {
                        if (const auto* pending = reconciler.Pending(path)) {
                            Defer(*pending); reconciler.Fail(path); deadlines.erase(path);
                        }
                        Log({{"event", "server_error"}, {"message", message}});
                    }
                    Expire();
                } catch (const std::exception& e) {
                    Log({{"event", "message_rejected"}, {"reason", e.what()}});
                }
            }
            Log({{"event", "disconnected"}, {"pending", reconciler.PendingCount()}});
        } catch (const std::exception& e) {
            try { Log({{"event", "failure"}, {"reason", e.what()}}); } catch (...) {}
        } catch (...) {}
        socket.Shutdown();
    }
    std::mutex mutex;
    std::vector<std::weak_ptr<ControlSource>> sources;
    std::atomic<bool> stop{false};
    WebSocket socket;
    StateReconciler reconciler;
    struct Baseline { Json value; double tolerance = 0; };
    std::map<std::string, Baseline> baselines;
    std::map<std::string, std::map<std::string, std::string>> ownerPaths;
    struct Retry { Json desired; std::string owner; std::uint64_t revision; std::chrono::steady_clock::time_point until; };
    std::map<std::string, Retry> retryAfter;
    std::uint64_t generation = 0;
    std::map<std::string, std::chrono::steady_clock::time_point> deadlines;
    std::ofstream log;
    std::size_t logBytes = 0;
    std::thread worker;
};
}
