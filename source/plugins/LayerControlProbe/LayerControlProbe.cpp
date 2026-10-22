#include "LayerControlProbe.h"
using namespace ffglex;

static CFFGLPluginInfo PluginInfo(
	PluginFactory< LayerControlProbe >,// Create method
	"RLCP",                      // Plugin unique ID of maximum length 4.
	"Layer Ctrl Probe",            // Plugin name
	2,                           // API major version number
	1,                           // API minor version number
	1,                           // Plugin major version number
	0,                           // Plugin minor version number
	FF_EFFECT,                   // Plugin type
	"Diagnostic only; no layer control",  // Plugin description
	"Resolume FFGL Example"      // About
);

static const char _vertexShaderCode[] = R"(#version 410 core
uniform vec2 MaxUV;

layout( location = 0 ) in vec4 vPosition;
layout( location = 1 ) in vec2 vUV;

out vec2 uv;

void main()
{
	gl_Position = vPosition;
	uv = vUV * MaxUV;
}
)";

static const char _fragmentShaderCode[] = R"(#version 410 core
uniform sampler2D InputTexture;


in vec2 uv;

out vec4 fragColor;

void main()
{
	fragColor = texture( InputTexture, uv );
}
)";

LayerControlProbe::LayerControlProbe()
{
    SetMinInputs(1); SetMaxInputs(1);
    SetOptionParamInfo(0, "Probe Mode", 2, 0.0f);
    SetParamElementInfo(0, 0, "Observe only", 0.0f);
    SetParamElementInfo(0, 1, "OSC on Connect TEST", 1.0f);
    probe.Record("create_trace_v3");
}
LayerControlProbe::~LayerControlProbe() { probe.Record("destroy"); }
unsigned int LayerControlProbe::Connect() {
    probe.Record("connect", mode.load() == 1);
    return FF_SUCCESS;
}
unsigned int LayerControlProbe::Disconnect() {
    probe.Record("disconnect"); return FF_SUCCESS;
}
void LayerControlProbe::SetHostInfo(const char* name, const char* version) {
    CFFGLPlugin::SetHostInfo(name, version);
    probe.Record("host_name", false, 0, 0, 0, name);
    probe.Record("host_version", false, 0, 0, 0, version);
}
FFResult LayerControlProbe::InitGL( const FFGLViewportStruct* vp )
{
    probe.Record("init_gl", false, vp->width, vp->height); firstFrame = true;
	if( !shader.Compile( _vertexShaderCode, _fragmentShaderCode ) )
	{
		DeInitGL();
		return FF_FAIL;
	}
	if( !quad.Initialise() )
	{
		DeInitGL();
		return FF_FAIL;
	}

	//Use base-class init as success result so that it retains the viewport.
	return CFFGLPlugin::InitGL( vp );
}
FFResult LayerControlProbe::ProcessOpenGL( ProcessOpenGLStruct* pGL )
{
    probe.Record("process", false, pGL ? pGL->numInputTextures : -1);
    if (!pGL) return FF_FAIL;
    if (firstFrame) { probe.Record("first_process"); firstFrame = false; }
	if( pGL->numInputTextures < 1 )
		return FF_FAIL;

	if( pGL->inputTextures[ 0 ] == NULL )
		return FF_FAIL;

	//FFGL requires us to leave the context in a default state on return, so use this scoped binding to help us do that.
	ScopedShaderBinding shaderBinding( shader.GetGLID() );
	//The shader's sampler is always bound to sampler index 0 so that's where we need to bind the texture.
	//Again, we're using the scoped bindings to help us keep the context in a default state.
	ScopedSamplerActivation activateSampler( 0 );
	Scoped2DTextureBinding textureBinding( pGL->inputTextures[ 0 ]->Handle );

	shader.Set( "InputTexture", 0 );

	//The input texture's dimension might change each frame and so might the content area.
	//We're adopting the texture's maxUV using a uniform because that way we dont have to update our vertex buffer each frame.
	FFGLTexCoords maxCoords = GetMaxGLTexCoords( *pGL->inputTextures[ 0 ] );
	shader.Set( "MaxUV", maxCoords.s, maxCoords.t );



	quad.Draw();

	return FF_SUCCESS;
}
FFResult LayerControlProbe::DeInitGL()
{
    probe.Record("deinit_gl");
	shader.FreeGLResources();
	quad.Release();

	return FF_SUCCESS;
}

FFResult LayerControlProbe::SetFloatParameter(unsigned int index, float value) {
    probe.Record("set_float", false, index, value);
    if (index != 0 || (value != 0.0f && value != 1.0f)) return FF_FAIL;
    mode.store(static_cast<int>(value));
    probe.Record(value == 0 ? "mode_observe" : "mode_connect_test");
    return FF_SUCCESS;
}
float LayerControlProbe::GetFloatParameter(unsigned int index) {
    probe.Record("get_float", false, index);
    return index == 0 ? static_cast<float>(mode.load()) : 0.0f;
}

// Record every callback, including repeated identical values. These observations
// are not interpreted as triggers and never request OSC transmission.
FFResult LayerControlProbe::SetTime(double time) {
    probe.Record("set_time", false, time);
    return CFFGLPlugin::SetTime(time);
}
void LayerControlProbe::SetBeatInfo(float tempo, float phase) {
    probe.Record("set_beat", false, tempo, phase);
    CFFGLPlugin::SetBeatInfo(tempo, phase);
}
void LayerControlProbe::SetSampleRate(unsigned int rate) {
    probe.Record("set_sample_rate", false, rate);
    CFFGLPlugin::SetSampleRate(rate);
}
unsigned int LayerControlProbe::Resize(const FFGLViewportStruct* vp) {
    if (!vp) { probe.Record("resize_null"); return FF_FAIL; }
    probe.Record("resize", false, vp->width, vp->height);
    return CFFGLPlugin::Resize(vp);
}
FFResult LayerControlProbe::GetInputStatus(unsigned int index) {
    probe.Record("get_input_status", false, index);
    return CFFGLPlugin::GetInputStatus(index);
}
char* LayerControlProbe::GetParameterDisplay(unsigned int index) {
    probe.Record("get_parameter_display", false, index);
    return CFFGLPlugin::GetParameterDisplay(index);
}
FFResult LayerControlProbe::SetTextParameter(unsigned int index, const char* value) {
    probe.Record("set_text", false, index, 0, 0, value);
    return CFFGLPlugin::SetTextParameter(index, value);
}
char* LayerControlProbe::GetTextParameter(unsigned int index) {
    probe.Record("get_text", false, index);
    return CFFGLPlugin::GetTextParameter(index);
}
