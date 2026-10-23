#include "../../control/DefinedPlugin.h"
#include "../../generated/LayerMaskControl.h"
using Plugin = control::DefinedPlugin<&definitions::LayerMaskControl>;
static const auto& definition = definitions::LayerMaskControl();
static CFFGLPluginInfo PluginInfo(Plugin::Create, definition.at("pluginId").get_ref<const std::string&>().c_str(),
    definition.at("name").get_ref<const std::string&>().c_str(), 2, 1, 0, 4,
    FF_EFFECT, "Composition-driven Layer synchronization", "JSON definition; see docs/SETUP.md");
