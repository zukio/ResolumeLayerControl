#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shlobj.h>
#include <objbase.h>
#include "Platform.h"
#include "ServerSettings.h"
#include <memory>
#include <stdexcept>
#include <cstdio>
namespace control {
std::filesystem::path HostServerSettingsPath() {
    wchar_t executable[32768];
    const auto count = GetModuleFileNameW(nullptr, executable, 32768);
    if (!count || count >= 32768) throw std::runtime_error("cannot locate host executable");
    if (_wcsicmp(std::filesystem::path(executable).filename().c_str(), L"Arena.exe") != 0)
        throw std::runtime_error("unsupported Resolume host (Arena required)");
    PWSTR raw = nullptr;
    const auto result = SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &raw);
    std::unique_ptr<wchar_t, decltype(&CoTaskMemFree)> documents(raw, CoTaskMemFree);
    if (FAILED(result)) throw std::runtime_error("cannot locate Documents folder");
    return ServerSettingsPath(documents.get());
}
std::filesystem::path LogPathFromEnvironment() {
    wchar_t path[32768];
    const auto length = GetEnvironmentVariableW(L"RLC_WS_LOG", path, 32768);
    if (length >= 32768) throw std::runtime_error("RLC_WS_LOG path too long");
    return length ? std::filesystem::path(path) : std::filesystem::path();
}
void DebugLog(const std::string& text) { OutputDebugStringA(text.c_str()); }
std::string NewInstanceKey() {
    GUID guid;
    if (FAILED(CoCreateGuid(&guid))) throw std::runtime_error("instance key generation failed");
    char text[40];
    std::snprintf(text,sizeof(text),"%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",guid.Data1,guid.Data2,guid.Data3,
        guid.Data4[0],guid.Data4[1],guid.Data4[2],guid.Data4[3],guid.Data4[4],guid.Data4[5],guid.Data4[6],guid.Data4[7]);
    return text;
}
}
