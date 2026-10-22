#pragma once
#include "ProbeWorker.h"
#include <FFGLSDK.h>
#include <atomic>
class LayerControlProbe : public CFFGLPlugin {
public:
    LayerControlProbe();
    ~LayerControlProbe();
    FFResult InitGL(const FFGLViewportStruct*) override;
    FFResult DeInitGL() override;
    FFResult ProcessOpenGL(ProcessOpenGLStruct*) override;
    unsigned int Connect() override;
    unsigned int Disconnect() override;
    FFResult SetTime(double) override;
    void SetBeatInfo(float, float) override;
    void SetSampleRate(unsigned int) override;
    unsigned int Resize(const FFGLViewportStruct*) override;
    FFResult GetInputStatus(unsigned int) override;
    char* GetParameterDisplay(unsigned int) override;
    FFResult SetTextParameter(unsigned int, const char*) override;
    char* GetTextParameter(unsigned int) override;
    void SetHostInfo(const char*, const char*) override;
    FFResult SetFloatParameter(unsigned int, float) override;
    float GetFloatParameter(unsigned int) override;
private:
    ProbeWorker probe;
    std::atomic<int> mode{0};
    bool firstFrame = true;
    ffglex::FFGLShader shader;
    ffglex::FFGLScreenQuad quad;
};
