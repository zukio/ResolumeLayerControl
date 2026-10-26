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
    void Composition(const Json& composition) {
        struct Candidate { std::shared_ptr<ControlSource> source; Evaluation evaluation; };
        std::vector<Candidate> candidates;
        std::vector<Intent> all;
        bool evaluationFailed = false;
        for (auto& source : Sources()) {
            try {
                auto evaluation = source->Evaluate(composition);
                Log({{"event", "gate"}, {"owner", source->Key()}, {"status", evaluation.status}, {"evidence", evaluation.evidence}});
                for (auto& intent : evaluation.intents) {
                    Log({{"event", "compare"}, {"owner", intent.owner}, {"path", intent.path}, {"actual", intent.actual}, {"desired", intent.desired}});
                    if (reconciler.ReadBack(intent.path, intent.actual)) {
                        deadlines.erase(intent.path);
                        Log({{"event", "readback"}, {"path", intent.path}, {"value", intent.actual}});
                    }
                    all.push_back(intent);
                }
                candidates.push_back({source, std::move(evaluation)});
            } catch (const std::exception& e) { evaluationFailed = true; Log({{"event", "invalid_state"}, {"owner", source->Key()}, {"reason", e.what()}}); }
        }
        for (const auto& intent : reconciler.Plan(all)) {
            if (stop.load()) return;
            const auto owner = std::find_if(candidates.begin(), candidates.end(), [&](const Candidate& c) { return c.source->Key() == intent.owner; });
            if (owner == candidates.end() || owner->source->Revision() != owner->evaluation.revision) continue;
            // The first value seen before our own change is the one to return to.
            baselines.emplace(intent.path, Baseline{intent.actual, intent.tolerance});
            Send(intent, "set_sent");
        }
        if (evaluationFailed) return;
        // Confirm in-flight requests (including restores) even when no source is active.
        for (const auto& path : reconciler.PendingPaths()) {
            const auto* current = FindParameter(composition, path);
            if (current && current->contains("value") && reconciler.ReadBack(path, (*current)["value"])) {
                deadlines.erase(path);
                Log({{"event", "readback"}, {"path", path}, {"value", (*current)["value"]}});
            }
        }
        // Restore only paths that no active source manages any more.
        std::set<std::string> active;
        for (const auto& intent : all) active.insert(intent.path);
        std::vector<Intent> restores;
        for (auto it = baselines.begin(); it != baselines.end();) {
            if (active.count(it->first)) { ++it; continue; }
            const auto* current = FindParameter(composition, it->first);
            if (!current || !current->contains("value")) { Log({{"event", "baseline_dropped"}, {"path", it->first}}); it = baselines.erase(it); continue; }
            const auto& actual = (*current)["value"];
            if (reconciler.ReadBack(it->first, actual)) deadlines.erase(it->first);
            if (Matches(actual, it->second.value, it->second.tolerance)) { it = baselines.erase(it); continue; }
            restores.push_back({"", it->first, actual, it->second.value, it->second.tolerance});
            ++it;
        }
        for (const auto& intent : reconciler.Plan(restores)) {
            if (stop.load()) return;
            baselines.erase(intent.path);
            Send(intent, "restore_sent");
        }
    }
    void Send(const Intent& intent, const char* event) {
        if (reconciler.TrackedCount() >= StateReconciler::Capacity) { Log({{"event", "pending_limit"}}); return; }
        const Json request = {{"action", "set"}, {"parameter", intent.path}, {"value", intent.desired}};
        if (!socket.Send(request.dump())) throw std::runtime_error("set transport failed; no retry");
        reconciler.Sent(intent);
        deadlines[intent.path] = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        Log({{"event", event}, {"owner", intent.owner}, {"path", intent.path}, {"actual", intent.actual}, {"desired", intent.desired}});
        if (!socket.Send(Json({{"action", "get"}, {"parameter", intent.path}}).dump()))
            throw std::runtime_error("readback request failed; no retry");
    }
    static const Json* FindParameter(const Json& node, const std::string& path) {
        static const std::string prefix = "/parameter/by-id/";
        if (path.compare(0, prefix.size(), prefix) != 0) return nullptr;
        const auto id = std::stoll(path.substr(prefix.size()));
        return FindById(node, id);
    }
    static const Json* FindById(const Json& node, long long id) {
        if (node.is_object()) {
            const auto found = node.find("id");
            if (found != node.end() && found->is_number_integer() && found->get<long long>() == id && node.contains("valuetype")) return &node;
            for (const auto& child : node.items())
                if (const auto* r = FindById(child.value(), id)) return r;
        } else if (node.is_array()) {
            for (const auto& child : node) if (const auto* r = FindById(child, id)) return r;
        }
        return nullptr;
    }
    void Run() noexcept {
        try {
            const auto path = LogPathFromEnvironment();
            if (!path.empty()) {
                log.open(path, std::ios::app);
                if (!log) throw std::runtime_error("cannot open RLC_WS_LOG");
            }
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
                const auto message = Json::parse(raw);
                // Timeouts only quarantine a request; never poll or resend. Evaluated on inbound traffic.
                const auto now = std::chrono::steady_clock::now();
                for (auto it = deadlines.begin(); it != deadlines.end();) {
                    if (now < it->second) { ++it; continue; }
                    reconciler.Fail(it->first);
                    Log({{"event", "readback_timeout"}, {"path", it->first}});
                    it = deadlines.erase(it);
                }
                if (IsComposition(message)) { Composition(message); continue; }
                const auto type = message.value("type", std::string());
                const auto path = message.value("path", std::string());
                if (type == "parameter_set") Log({{"event", "server_response"}, {"message", message}});
                if ((type == "parameter_get" || type == "parameter_update") && message.contains("value") &&
                    reconciler.ReadBack(path, message["value"])) {
                    deadlines.erase(path);
                    Log({{"event", "readback"}, {"path", path}, {"value", message["value"]}});
                }
                // Individual updates (including position) never start reconciliation.
                if (message.contains("error") && !message["error"].is_null()) {
                    if (!path.empty()) { reconciler.Fail(path); deadlines.erase(path); }
                    Log({{"event", "server_error"}, {"message", message}});
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
    std::map<std::string, std::chrono::steady_clock::time_point> deadlines;
    std::ofstream log;
    std::size_t logBytes = 0;
    std::thread worker;
};
}
