#include "../../control/APISenderSource.h"
#include "../../control/PassThrough.h"
#include <array>

class APISender final : public CFFGLPlugin {
    using Source = control::APISenderSource;
public:
    APISender() : source(std::make_shared<Source>()) {
        SetMinInputs(1); SetMaxInputs(1);
        SetParamInfo(Source::Enabled, "Enabled", FF_TYPE_BOOLEAN, true);
        SetParamInfo(Source::ActiveCommand, "Active Command", FF_TYPE_TEXT, "");
        SetParamInfo(Source::InactiveCommand, "Inactive Command", FF_TYPE_TEXT, "");
        SetParamInfo(Source::Identity, "Instance Key", FF_TYPE_TEXT, "");
        RaiseParamEvent(Source::Identity, FF_EVENT_FLAG_VALUE);
        runtime = control::Runtime::Acquire(); runtime->Add(source);
    }
    ~APISender() override { source->Close(); source.reset(); runtime.reset(); }
    FFResult InitGL(const FFGLViewportStruct* vp) override {
        try { return vp && image.Init() ? CFFGLPlugin::InitGL(vp) : FF_FAIL; } catch (...) { return FF_FAIL; }
    }
    FFResult DeInitGL() override { try { image.Free(); return FF_SUCCESS; } catch (...) { return FF_FAIL; } }
    FFResult ProcessOpenGL(ProcessOpenGLStruct* p) override { try { return image.Draw(p); } catch (...) { return FF_FAIL; } }
    FFResult SetFloatParameter(unsigned id, float value) override {
        try { return id == Source::Enabled && source->SetEnabled(value) ? FF_SUCCESS : FF_FAIL; } catch (...) { return FF_FAIL; }
    }
    float GetFloatParameter(unsigned id) override { try { return id == Source::Enabled ? source->GetEnabled() : 0; } catch (...) { return 0; } }
    FFResult SetTextParameter(unsigned id, const char* text) override {
        try {
            if (!source->SetText(id, text ? text : "")) return FF_FAIL;
            if (id == Source::Identity) RaiseParamEvent(id, FF_EVENT_FLAG_VALUE);
            return FF_SUCCESS;
        } catch (...) { return FF_FAIL; }
    }
    char* GetTextParameter(unsigned id) override {
        try {
            if (id == Source::Enabled || id >= Source::Count) return nullptr;
            texts[id] = source->GetText(id);
            return const_cast<char*>(texts[id].c_str());
        } catch (...) { return nullptr; }
    }
    static FFResult __stdcall Create(CFFGLPlugin** result) {
        if (!result) return FF_FAIL;
        *result = nullptr;
        try { *result = new APISender; return FF_SUCCESS; } catch (...) { return FF_FAIL; }
    }
private:
    std::shared_ptr<Source> source;
    std::shared_ptr<control::Runtime> runtime;
    control::PassThrough image;
    std::array<std::string, Source::Count> texts;
};
static CFFGLPluginInfo PluginInfo(APISender::Create, "RAPI", "APISender", 2, 1, 0, 1,
    FF_EFFECT, "Send Resolume WebSocket JSON on Clip activation and deactivation", "See docs/APISender.md");
