#include "../source/control/DefinitionPolicy.h"
#include "../source/generated/LayerControl.h"
#include "../source/generated/LayerMaskControl.h"
#include <fstream>
#include <iostream>
#include <limits>

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
        Check(r.Plan(competing).empty(), "multiple active writers blocked");
        r.Fail("/parameter/by-id/103");
        Check(r.Plan(Evaluate(c, s)).empty(), "failed request not retried");
        r.Reset();
        Check(!Valid(definitions::LayerControl(), Duration, std::numeric_limits<float>::quiet_NaN()), "NaN rejected");
        Check(!Valid(definition, TargetLayer, 1.5f) && !Valid(definition, TargetLayer, 0), "bad layer rejected");
        Check(!IsComposition(Json{{"type", "parameter_update"}, {"path", "/composition/layers/1/clips/1/transport/position"}}), "position never triggers");
        s[TargetLayer] = 2;
        bool failed = false; try { Evaluate(c, s); } catch (...) { failed = true; }
        Check(failed, "missing layer fails closed");
        s[TargetLayer] = 1; c["layergroups"].push_back({{"id", 5}});
        failed = false; try { Evaluate(c, s); } catch (...) { failed = true; }
        Check(failed, "unvalidated groups fail closed");
        c = Fixture(); c["layers"][0]["maskmode"].erase("id");
        failed = false; try { Evaluate(c, s); } catch (...) { failed = true; }
        Check(failed, "missing real id fails closed");
        r.Sent({"precision", "duration", 0, 0.10000000149, 0.000001});
        Check(r.ReadBack("duration", 0.1), "duration readback float precision");
        for (unsigned i = 0; i < StateReconciler::Capacity; ++i) r.Fail(std::to_string(i));
        Check(r.TrackedCount() == StateReconciler::Capacity, "failed tracking bounded");
        r.Fail("overflow");
        Check(r.TrackedCount() == StateReconciler::Capacity, "failed tracking cannot grow");
        if (argc == 2) {
            // Parse actual, unmodified capture. No synthetic fixture can pass host acceptance.
            std::ifstream file(argv[1]); Check(file.good(), "capture opens");
            const auto composition = Json::parse(file);
            Check(IsComposition(composition), "real composition snapshot");
            const auto& layer = composition["layers"][0];
            Check(Choice("capture", layer.at("transition").at("blend_mode"), "Alpha").desired == "Alpha", "captured transition schema");
            Check(Choice("capture", layer.at("maskmode"), "Disabled").desired == "Disabled", "captured mask schema");
            Check(!ResolveClip(composition, "test-key").clip, "unknown key is not resolved");
        }
        std::cout << checks << " checks passed\n"; return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
