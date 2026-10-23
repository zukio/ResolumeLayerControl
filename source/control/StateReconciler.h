#pragma once
#include <nlohmann/json.hpp>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <cmath>

namespace control {
using Json = nlohmann::json;
// Domain-independent absolute-value intent. IDs are resolved from each snapshot.
struct Intent {
    std::string owner, path;
    Json actual, desired;
    double tolerance = 0;
};
inline bool Matches(const Json& actual, const Json& desired, double tolerance = 0) {
    if (actual.is_number() && desired.is_number() && tolerance > 0)
        return std::abs(actual.get<double>() - desired.get<double>()) <= tolerance;
    return actual == desired;
}
class StateReconciler {
public:
    // No outbound queue: candidates expire with the notification that created them.
    std::vector<Intent> Plan(const std::vector<Intent>& intents) const {
        if (pending.size() + failed.size() >= Capacity) return {};
        std::map<std::string, unsigned> owners;
        for (const auto& i : intents) ++owners[i.path];
        std::vector<Intent> result;
        for (const auto& i : intents)
            if (owners[i.path] == 1 && !i.actual.is_null() && !Matches(i.actual, i.desired, i.tolerance) &&
                pending.count(i.path) == 0 && failed.count(i.path) == 0)
                result.push_back(i);
        return result;
    }
    void Sent(const Intent& intent) {
        if (pending.size() + failed.size() >= Capacity) throw std::runtime_error("request tracking limit");
        pending[intent.path] = intent;
    }
    bool ReadBack(const std::string& path, const Json& value) {
        auto it = pending.find(path);
        if (it == pending.end() || !Matches(value, it->second.desired, it->second.tolerance)) return false;
        pending.erase(it);
        return true;
    }
    // A failure is latched for the connection: no speculative automatic retries.
    void Fail(const std::string& path) {
        pending.erase(path);
        if (failed.size() < Capacity) failed.insert(path);
    }
    void Reset() { pending.clear(); failed.clear(); }
    std::size_t PendingCount() const { return pending.size(); }
    std::vector<std::string> PendingPaths() const {
        std::vector<std::string> result;
        for (const auto& item : pending) result.push_back(item.first);
        return result;
    }
    std::size_t TrackedCount() const { return pending.size() + failed.size(); }
    static constexpr std::size_t Capacity = 384;
private:
    std::map<std::string, Intent> pending;
    std::set<std::string> failed;
};
}
