// Linux-only harness for cloud tests; never linked into a production plugin.
#include "../source/control/Platform.h"
#include <atomic>
#include <cstdlib>
#include <stdexcept>
namespace control {
std::filesystem::path HostServerSettingsPath() { throw std::runtime_error("test settings path required"); }
std::filesystem::path LogPathFromEnvironment() {
    const auto* path = std::getenv("RLC_WS_LOG");
    return path ? std::filesystem::u8path(path) : std::filesystem::path();
}
void DebugLog(const std::string&) {}
std::string NewInstanceKey() {
    static std::atomic<unsigned> counter{0};
    return "cloud-test-" + std::to_string(++counter);
}
}
