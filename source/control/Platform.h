#pragma once
#include <filesystem>
#include <string>
namespace control {
std::filesystem::path HostServerSettingsPath();
std::filesystem::path LogPathFromEnvironment();
void DebugLog(const std::string& text);
std::string NewInstanceKey();
}
