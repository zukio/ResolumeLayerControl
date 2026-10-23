#pragma once
#define NOMINMAX
#include <objbase.h>
#include "PassThrough.h"
#include "Runtime.h"
#include "DefinitionPolicy.h"
#include <cstdio>
namespace control {
class DefinedSource final : public ControlSource {
public:
    explicit DefinedSource(const Json& d) : definition(d), settings(Defaults(d)) {
        GUID guid;
        if (FAILED(CoCreateGuid(&guid))) throw std::runtime_error("instance key generation failed");
        char text[40];
        std::snprintf(text,sizeof(text),"%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",guid.Data1,guid.Data2,guid.Data3,
            guid.Data4[0],guid.Data4[1],guid.Data4[2],guid.Data4[3],guid.Data4[4],guid.Data4[5],guid.Data4[6],guid.Data4[7]);
        SetKey(text);
    }
    Evaluation Evaluate(const Json& c) override {
        std::lock_guard<std::mutex> lock(mutex);
        Evaluation r; r.revision = revision.load();
        if (!alive) { r.status = "destroyed"; return r; }
        const auto key = Key();
        r.intents = BuildIntents(definition,c,key,settings,r.status);
        r.evidence["plugin"] = definition.at("name");
        r.evidence["target_layer"] = settings[RoleIndex(definition,"target")];
        auto resolved = ResolveClip(c,key);
        if (resolved.clip) { r.evidence["clip_id"] = resolved.clip->at("id"); r.evidence["connected"] = resolved.clip->at("connected"); }
        return r;
    }
    std::uint64_t Revision() const override { return revision.load(); }
    bool Set(unsigned i,float v) {
        if (!Valid(definition,i,v)) return false;
        std::lock_guard<std::mutex> lock(mutex); settings[i] = v; ++revision; return true;
    }
    float Get(unsigned i) { std::lock_guard<std::mutex> lock(mutex); return i < settings.size() ? settings[i] : 0; }
    void Adopt(const std::string& value) {
        std::lock_guard<std::mutex> lock(mutex);
        if (Key() == value) return;
        SetKey(value); ++revision;
    }
    void Close() { std::lock_guard<std::mutex> lock(mutex); alive = false; ++revision; }
private:
    const Json& definition;
    std::mutex mutex;
    Settings settings;
    std::atomic<std::uint64_t> revision{0};
    bool alive = true;
};
template<const Json& (*Definition)()> class DefinedPlugin final : public CFFGLPlugin {
public:
    DefinedPlugin() : source(std::make_shared<DefinedSource>(Definition())) {
        SetMinInputs(1); SetMaxInputs(1);
        for (const auto& p : Definition().at("parameters")) {
            const auto id = p.at("id").get<unsigned>();
            const auto label = p.at("label").get<std::string>();
            const auto type = p.at("type").get<std::string>();
            if (type == "text") SetParamInfo(id,label.c_str(),FF_TYPE_TEXT,"");
            else if (type == "boolean") SetParamInfo(id,label.c_str(),FF_TYPE_BOOLEAN,p.at("default").get<float>() != 0);
            else if (type == "choice") {
                const auto options = p.at("options").get<std::vector<std::string>>();
                SetOptionParamInfo(id,label.c_str(),static_cast<unsigned>(options.size()),p.at("default").get<float>());
                for (unsigned i = 0; i < options.size(); ++i) SetParamElementInfo(id,i,options[i].c_str(),static_cast<float>(i));
            } else {
                SetParamInfo(id,label.c_str(),type == "integer" ? FF_TYPE_INTEGER : FF_TYPE_STANDARD,p.at("default").get<float>());
                SetParamRange(id,p.at("min").get<float>(),p.at("max").get<float>());
            }
            SetParamVisibility(id,p.value("visible",true) && p.value("role","") != "reserved",false);
        }
        identity = RoleIndex(Definition(),"identity");
        RaiseParamEvent(identity,FF_EVENT_FLAG_VALUE);
        runtime = Runtime::Acquire(); runtime->Add(source);
    }
    ~DefinedPlugin() override { source->Close(); source.reset(); runtime.reset(); }
    FFResult InitGL(const FFGLViewportStruct* vp) override { try { if (!vp || !image.Init()) return FF_FAIL; return CFFGLPlugin::InitGL(vp); } catch (...) { return FF_FAIL; } }
    FFResult DeInitGL() override { try { image.Free(); return FF_SUCCESS; } catch (...) { return FF_FAIL; } }
    FFResult ProcessOpenGL(ProcessOpenGLStruct* p) override { try { return image.Draw(p); } catch (...) { return FF_FAIL; } }
    FFResult SetFloatParameter(unsigned i,float v) override { try { return source->Set(i,v) ? FF_SUCCESS : FF_FAIL; } catch (...) { return FF_FAIL; } }
    float GetFloatParameter(unsigned i) override { try { return source->Get(i); } catch (...) { return 0; } }
    // Adopt the key the host restored from the saved Composition; otherwise it never matches the stored one.
    FFResult SetTextParameter(unsigned i,const char* text) override {
        if (i != identity) return FF_FAIL;
        try {
            if (text && *text) source->Adopt(text);
            keyText = source->Key();
            RaiseParamEvent(identity,FF_EVENT_FLAG_VALUE);
            return FF_SUCCESS;
        } catch (...) { return FF_FAIL; }
    }
    char* GetTextParameter(unsigned i) override { if (i != identity) return nullptr; keyText = source->Key(); return const_cast<char*>(keyText.c_str()); }
    static FFResult __stdcall Create(CFFGLPlugin** result) {
        if (!result) return FF_FAIL;
        *result = nullptr;
        try { *result = new DefinedPlugin; return FF_SUCCESS; } catch (...) { return FF_FAIL; }
    }
private:
    std::shared_ptr<DefinedSource> source;
    std::shared_ptr<Runtime> runtime;
    PassThrough image;
    std::string keyText;
    unsigned identity = 0;
};
}
