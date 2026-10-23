#pragma once
#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <xmllite.h>
#include <wrl/client.h>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "xmllite.lib")

namespace control {
struct ServerSettings {
    bool enabled;
    unsigned short port;
};

// Only the Arena layout has been measured. Do not use another product's settings.
inline std::filesystem::path ServerSettingsPath(const std::filesystem::path& documents,
                                               const std::filesystem::path& executable) {
    if (_wcsicmp(executable.filename().c_str(), L"Arena.exe") != 0)
        throw std::runtime_error("unsupported Resolume host (Arena required)");
    return documents / L"Resolume Arena" / L"Preferences" / L"server.xml";
}

inline std::filesystem::path HostServerSettingsPath() {
    wchar_t executable[32768];
    const auto count = GetModuleFileNameW(nullptr, executable, 32768);
    if (!count || count >= 32768) throw std::runtime_error("cannot locate host executable");
    PWSTR raw = nullptr;
    const auto result = SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &raw);
    std::unique_ptr<wchar_t, decltype(&CoTaskMemFree)> documents(raw, CoTaskMemFree);
    if (FAILED(result)) throw std::runtime_error("cannot locate Documents folder");
    return ServerSettingsPath(documents.get(), executable);
}

inline ServerSettings ReadServerSettings(const std::filesystem::path& file) {
    // Bound the actual read too, so a file changing during startup cannot grow without limit.
    std::ifstream input(file, std::ios::binary);
    if (!input) throw std::runtime_error("cannot open Resolume server.xml");
    std::string xml(65537, '\0');
    input.read(xml.data(), static_cast<std::streamsize>(xml.size()));
    xml.resize(static_cast<size_t>(input.gcount()));
    if (input.bad() || xml.empty() || xml.size() > 65536)
        throw std::runtime_error("invalid server.xml size/read (limit 64 KiB)");
    Microsoft::WRL::ComPtr<IStream> stream;
    stream.Attach(SHCreateMemStream(reinterpret_cast<const BYTE*>(xml.data()), static_cast<UINT>(xml.size())));
    Microsoft::WRL::ComPtr<IXmlReader> reader;
    if (!stream || FAILED(CreateXmlReader(__uuidof(IXmlReader), reinterpret_cast<void**>(reader.GetAddressOf()), nullptr)))
        throw std::runtime_error("cannot create server.xml reader");
    if (FAILED(reader->SetProperty(XmlReaderProperty_DtdProcessing, DtdProcessing_Prohibit)) ||
        FAILED(reader->SetProperty(XmlReaderProperty_MaxElementDepth, 16)) ||
        FAILED(reader->SetInput(stream.Get())))
        throw std::runtime_error("cannot initialize server.xml reader");
    auto attribute = [&reader](const wchar_t* name) {
        if (reader->MoveToAttributeByName(name, nullptr) != S_OK)
            throw std::runtime_error("missing server.xml attribute");
        const wchar_t* value = nullptr;
        if (FAILED(reader->GetValue(&value, nullptr))) throw std::runtime_error("invalid server.xml attribute");
        std::wstring text(value);
        reader->MoveToElement();
        return text;
    };
    bool root = false;
    ServerSettings settings{};
    XmlNodeType type;
    HRESULT result;
    while ((result = reader->Read(&type)) == S_OK) {
        if (type != XmlNodeType_Element) continue;
        if (root) continue;
        const wchar_t* name = nullptr;
        const wchar_t* uri = nullptr;
        if (FAILED(reader->GetQualifiedName(&name, nullptr)) ||
            FAILED(reader->GetNamespaceUri(&uri, nullptr)) ||
            std::wstring(name) != L"ServerController" || *uri)
            throw std::runtime_error("unexpected server.xml root");
        root = true;
        const auto enabled = attribute(L"enabled");
        const auto port = attribute(L"port");
        const auto address = attribute(L"address");
        if (enabled != L"0" && enabled != L"1") throw std::runtime_error("invalid Webserver enabled flag");
        if (port.empty() || port.size() > 5 || port.find_first_not_of(L"0123456789") != std::wstring::npos)
            throw std::runtime_error("invalid Webserver port");
        const auto number = std::stoul(port);
        if (number < 1 || number > 65535) throw std::runtime_error("Webserver port out of range");
        if (address != L"0.0.0.0" && address != L"127.0.0.1")
            throw std::runtime_error("Webserver listen address does not support 127.0.0.1 connection");
        settings = {enabled == L"1", static_cast<unsigned short>(number)};
    }
    if (FAILED(result) || !root) throw std::runtime_error("malformed server.xml");
    return settings;
}
}
