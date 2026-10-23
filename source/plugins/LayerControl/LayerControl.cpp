#include "../../control/DefinedPlugin.h"
#include "../../generated/LayerControl.h"
using Plugin = control::DefinedPlugin<&definitions::LayerControl>;
static const auto& definition = definitions::LayerControl();
static CFFGLPluginInfo PluginInfo(Plugin::Create, definition.at("pluginId").get_ref<const std::string&>().c_str(),
    definition.at("name").get_ref<const std::string&>().c_str(), 2, 1, 0, 4,
    FF_EFFECT, "Composition-driven Layer synchronization", "JSON definition; see docs/SETUP.md");
