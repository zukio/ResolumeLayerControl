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
    std::string property;
    bool force = false;
    std::uint64_t revision = 0;
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
        if (pending.size() >= Capacity) return {};
        std::map<std::string, Intent> unique;
        std::set<std::string> conflicts;
        for (const auto& i : intents) {
            auto inserted = unique.emplace(i.path, i);
            if (!inserted.second && (inserted.first->second.desired != i.desired ||
                inserted.first->second.actual != i.actual)) conflicts.insert(i.path);
        }
        std::vector<Intent> result;
        for (const auto& entry : unique) {
            const auto& i = entry.second;
            if (!conflicts.count(i.path) && !i.actual.is_null() &&
                (i.force || !Matches(i.actual, i.desired, i.tolerance)) && !pending.count(i.path))
                result.push_back(i);
        }
        return result;
    }
    const Intent* Pending(const std::string& path) const {
        auto it = pending.find(path); return it == pending.end() ? nullptr : &it->second;
    }
    void Sent(const Intent& intent) {
        if (pending.size() >= Capacity) throw std::runtime_error("request tracking limit");
        pending[intent.path] = intent;
    }
    bool ReadBack(const std::string& path, const Json& value) {
        auto it = pending.find(path);
        if (it == pending.end() || !Matches(value, it->second.desired, it->second.tolerance)) return false;
        pending.erase(it);
        return true;
    }
    // Release only this request. Runtime rate-limits recovery from fresh snapshots.
    void Fail(const std::string& path) { pending.erase(path); }
    void Reset() { pending.clear(); }
    std::size_t PendingCount() const { return pending.size(); }
    std::vector<std::string> PendingPaths() const {
        std::vector<std::string> result;
        for (const auto& item : pending) result.push_back(item.first);
        return result;
    }
    std::size_t TrackedCount() const { return pending.size(); }
    static constexpr std::size_t Capacity = 384;
private:
    std::map<std::string, Intent> pending;
};
}
