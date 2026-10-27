#include "../source/control/DefinitionPolicy.h"
#include "../source/generated/LayerControl.h"
#include "../source/generated/LayerMaskControl.h"
#include <fstream>
#include <iostream>
#include <limits>
#include <chrono>

using namespace control;
static Json definition = definitions::LayerControl();
static constexpr unsigned Enabled=0, TargetLayer=1, Duration=2, MaskMode=2;
static unsigned checks = 0;
static void Check(bool ok, const char* label) { ++checks; if (!ok) throw std::runtime_error(label); }
static Json Fixture() {
    return {{"layergroups", Json::array()}, {"layers", Json::array({{
        {"id", 100}, {"transition", {
            {"blend_mode", {{"id", 101}, {"valuetype", "ParamChoice"}, {"value", "Alpha"}, {"options", {"Cut", "Alpha"}}}},
            {"duration", {{"id", 102}, {"valuetype", "ParamRange"}, {"value", 1.0}, {"min", 0}, {"max", 10}}}}},
        {"maskmode", {{"id", 103}, {"valuetype", "ParamChoice"}, {"value", "Disabled"}, {"options", {"Disabled", "One Below", "All Below"}}}},
        {"clips", Json::array({{{"id", 200}, {"connected", {{"value", "Connected"}}},
            {"video", {{"effects", Json::array({{{"bypassed", {{"value", false}}}, {"params", {{"Instance Key", {{"value", "test-key"}}}}}}})}}}}})}
    }})}};
}
static std::vector<Intent> Evaluate(const Json& c, const Settings& s) {
    std::string status; return BuildIntents(definition, c, "test-key", s, status);
}
int main(int argc, char** argv) {
    try {
        {
            auto large = Fixture(); large["layers"] = Json::array();
            for (int l = 0; l < 30; ++l) {
                auto layer = Fixture()["layers"][0]; layer["id"] = 1000 + l;
                layer["clips"] = Json::array();
                for (int col = 0; col < 30; ++col) {
                    auto clip = Fixture()["layers"][0]["clips"][0];
                    clip["id"] = 10000 + l * 30 + col;
                    clip["connected"]["value"] = col == 0 ? "Connected" : "Disconnected";
                    clip["video"]["effects"][0]["params"]["Instance Key"]["value"] = std::to_string(l * 30 + col);
                    layer["clips"].push_back(clip);
                }
                large["layers"].push_back(layer);
            }
            large["layergroups"] = Json::array({{{"layers", large["layers"]}}});
            auto compact = ParseStateMessage(large.dump());
            Check(!compact["layergroups"][0].contains("layers"), "group duplicate DOM skipped");
            Check(compact["layers"] == large["layers"], "canonical layers unchanged by parsing");
            const auto start = std::chrono::steady_clock::now();
            CompositionIndex index(large);
            unsigned activeCount = 0;
            for (int i = 0; i < 900; ++i) {
                auto values = Defaults(definitions::LayerControl()); values[TargetLayer] = static_cast<float>(i / 30 + 1);
                std::string status; std::set<std::string> held; Json errors;
                auto intents = BuildIntents(definitions::LayerControl(), large, std::to_string(i), values, status, &held, &errors, &index);
                Check(index.Resolve(std::to_string(i)).clip != nullptr, "900 unique clips resolve with group aliases");
                if (!intents.empty()) ++activeCount;
            }
            Check(activeCount == 30, "30x30 evaluates exactly the 30 active clips");
            const auto indexed = std::chrono::steady_clock::now();
            for (int i = 0; i < 900; ++i)
                Check(ResolveClip(large, std::to_string(i)).clip == index.Resolve(std::to_string(i)).clip, "indexed and reference resolution agree");
            std::cout << "30x30 / 900 sources: indexed evaluation us=" << std::chrono::duration_cast<std::chrono::microseconds>(indexed-start).count()
                      << ", previous resolver only us=" << std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-indexed).count() << '\n';
            large["layers"][1]["clips"][1]["video"]["effects"][0]["params"]["Instance Key"]["value"] = "0";
            Check(!CompositionIndex(large).Resolve("0").clip, "indexed duplicate nonactive key rejected");
            large["video"] = large["layers"][0]["clips"][2]["video"];
            Check(!CompositionIndex(large).Resolve("2").clip, "indexed composition effect counted");
        }
        auto c = Fixture(); auto s = Defaults(definition); StateReconciler r;
        Check(!Evaluate(c, s).empty(), "default enabled");
        s[Enabled] = 0;
        Check(Evaluate(c, s).empty(), "disabled sends nothing");
        s[Enabled] = 1;
        Check(r.Plan(Evaluate(c, s)).empty(), "already matching");
        s[Duration] = 2;
        auto plan = r.Plan(Evaluate(c, s));
        Check(plan.size() == 1 && plan[0].path == "/parameter/by-id/102", "only duration differs");
        r.Sent(plan[0]);
        Check(r.Plan(Evaluate(c, s)).empty(), "in-flight duplicate suppressed");
        Check(!r.ReadBack(plan[0].path, 1), "wrong readback not confirmed");
        Check(r.ReadBack(plan[0].path, 2), "readback confirmed");
        c["layers"][0]["transition"]["duration"]["value"] = 2;
        Check(r.Plan(Evaluate(c, s)).empty(), "self notification converges");
        c["layers"][0]["transition"]["duration"]["value"] = 3;
        Check(r.Plan(Evaluate(c, s)).size() == 1, "manual override compared with actual");
        auto& clip = c["layers"][0]["clips"][0];
        for (const auto* state : {"Disconnected", "Previewing", "Empty", "Unknown"}) {
            clip["connected"]["value"] = state;
            Check(Evaluate(c, s).empty(), "inactive or unknown blocked");
        }
        clip["connected"]["value"] = "Connected & previewing";
        Check(!Evaluate(c, s).empty(), "connected and previewing active");
        c["layers"][0]["active_clip"] = clip;
        Check(!Evaluate(c, s).empty(), "active_clip alias is not a duplicate placement");
        auto duplicate = clip;
        c["layers"][0]["clips"].push_back(duplicate);
        Check(Evaluate(c, s).empty(), "duplicate identity blocked including inactive clips");
        c["layers"][0]["clips"].erase(1);
        c["layers"][0]["clips"][0]["video"]["effects"][0]["bypassed"]["value"] = true;
        Check(Evaluate(c, s).empty(), "bypass blocks");
        c = Fixture();
        auto transition = Evaluate(c, s);
        Check(transition.size() == 2, "transition plugin has only two bindings");
        for (const auto& i : transition) Check(i.path != "/parameter/by-id/103", "transition never changes Mask");
        definition = definitions::LayerMaskControl(); s = Defaults(definition);
        Check(Evaluate(c,s).empty(), "mask disabled by default");
        s[Enabled] = 1;
        c["layers"][0]["maskmode"]["value"] = "One Below";
        plan = r.Plan(Evaluate(c, s));
        Check(plan.size() == 1 && plan[0].desired == "Disabled", "disabled actively restores normal video");
        s[MaskMode] = 2;
        Check(Evaluate(c, s)[0].desired == "All Below", "mask enum mapping");
        auto competing = Evaluate(c, s); competing.push_back(competing[0]); competing[1].owner = "another";
        Check(r.Plan(competing).size() == 1, "same-value owners share one request");
        competing[1].desired = "Disabled";
        Check(r.Plan(competing).empty(), "different-value owners are deferred");
        r.Fail("/parameter/by-id/103");
        Check(r.Plan(Evaluate(c, s)).size() == 1, "failure does not permanently reject fresh intents");
        r.Reset();
        Check(!Valid(definitions::LayerControl(), Duration, std::numeric_limits<float>::quiet_NaN()), "NaN rejected");
        Check(!Valid(definition, TargetLayer, 1.5f) && !Valid(definition, TargetLayer, 0), "bad layer rejected");
        Check(!IsComposition(Json{{"type", "parameter_update"}, {"path", "/composition/layers/1/clips/1/transport/position"}}), "position never triggers");
        s[TargetLayer] = 2;
        bool failed = false; try { Evaluate(c, s); } catch (...) { failed = true; }
        Check(failed, "missing layer fails closed");
        s[TargetLayer] = 1; c["layergroups"].push_back({{"id", 5}, {"layers", Json::array()}});
        Check(!Evaluate(c, s).empty(), "group existence does not block outside layer");
        c["layergroups"][0]["layers"].push_back(c["layers"][0]);
        Check(!Evaluate(c, s).empty(), "group member alias is not duplicate instance");
        c["layergroups"][0]["layers"][0]["id"] = 999;
        failed = false; try { Evaluate(c, s); } catch (...) { failed = true; }
        Check(!failed, "unrelated stale group references do not block canonical layer");
        c["layergroups"][0]["layers"][0] = c["layers"][0];
        c["layergroups"][0]["layers"][0]["maskmode"]["value"] = "All Below";
        failed = false; try { Evaluate(c, s); } catch (...) { failed = true; }
        Check(!failed, "dynamic group alias differences do not block");
        c = Fixture();
        auto other = c["layers"][0]; other["id"] = 300;
        other["clips"] = Json::array(); other["maskmode"]["id"] = 303;
        c["layers"].push_back(other);
        c["layergroups"] = Json::array({{{"id", 5}, {"layers", Json::array({other})}}});
        s[TargetLayer] = 2;
        Check(Evaluate(c, s)[0].path == "/parameter/by-id/303", "group target uses global layer number");
        std::swap(c["layers"][0], c["layers"][1]);
        Check(Evaluate(c, s)[0].path == "/parameter/by-id/103", "reorder resolves current number and parameter id");
        c["layergroups"] = Json::array();
        Check(Evaluate(c, s)[0].path == "/parameter/by-id/103", "ungroup does not shift layer number");
        c["layers"][0]["clips"] = c["layers"][1]["clips"];
        c["layers"].erase(1);
        failed = false; try { Evaluate(c, s); } catch (...) { failed = true; }
        Check(failed, "deleted target never falls back to another layer");
        s[TargetLayer] = 1;
        c = Fixture(); c["layers"][0]["maskmode"].erase("id");
        failed = false; try { Evaluate(c, s); } catch (...) { failed = true; }
        Check(failed, "missing real id fails closed");
        r.Sent({"precision", "duration", 0, 0.10000000149, 0.000001});
        Check(r.ReadBack("duration", 0.1), "duration readback float precision");
        for (unsigned i = 0; i < StateReconciler::Capacity; ++i) r.Fail(std::to_string(i));
        Check(r.TrackedCount() == 0, "failed history does not consume pending capacity");
        r.Fail("overflow");
        Check(r.TrackedCount() == 0, "unrelated errors do not consume capacity");
        {
            auto partial = Fixture(); auto values = Defaults(definitions::LayerControl());
            partial["layers"][0]["transition"]["duration"].erase("id");
            std::set<std::string> held; Json errors; std::string status;
            auto valid = BuildIntents(definitions::LayerControl(), partial, "test-key", values, status, &held, &errors);
            Check(valid.size() == 1 && valid[0].path == "/parameter/by-id/101", "bad duration does not block blend");
            Check(status == "partial" && held.count("transition.duration"), "only failed property held");
            partial["layers"][0]["transition"]["duration"]["id"] = 102;
            held.clear(); errors.clear();
            valid = BuildIntents(definitions::LayerControl(), partial, "test-key", values, status, &held, &errors);
            Check(valid.size() == 2 && held.empty(), "normal metadata recovers on next evaluation");
            auto grouped = Fixture();
            grouped["layergroups"] = Json::array({{{"layers", Json::array({grouped["layers"][0]})},
                {"video", grouped["layers"][0]["clips"][0]["video"]}}});
            Check(!ResolveClip(grouped, "test-key").clip, "real group effect placement still detects duplicate key");
        }
        if (argc >= 2) {
            // Parse actual, unmodified capture. No synthetic fixture can pass host acceptance.
            std::ifstream file(argv[1]); Check(file.good(), "capture opens");
            const auto composition = Json::parse(file);
            Check(IsComposition(composition), "real composition snapshot");
            const auto& layer = composition["layers"][0];
            Check(Choice("capture", layer.at("transition").at("blend_mode"), "Alpha").desired == "Alpha", "captured transition schema");
            Check(Choice("capture", layer.at("maskmode"), "Disabled").desired == "Disabled", "captured mask schema");
            Check(!ResolveClip(composition, "test-key").clip, "unknown key is not resolved");
        }
        if (argc >= 3) {
            std::ifstream file(argv[2]); Check(file.good(), "group capture opens");
            const auto captured = Json::parse(file);
            Check(captured.at("layergroups").size() > 0, "real group capture");
            Check(CanonicalTokenOccurrences(captured, "missing-key") == 0, "real group aliases validate");
            for (const auto& layer : captured.at("layers"))
                for (const auto& clip : layer.at("clips")) {
                    if (!clip.contains("video") || !clip["video"].is_object()) continue;
                    for (const auto& effect : clip["video"].at("effects")) {
                        if (!effect.contains("params") || !effect["params"].contains("Instance Key")) continue;
                        const auto key = effect["params"]["Instance Key"]["value"].get<std::string>();
                        Check(ResolveClip(captured, key).clip != nullptr, "captured instance resolves with group present");
                    }
                }
        }
        std::cout << checks << " checks passed\n"; return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
