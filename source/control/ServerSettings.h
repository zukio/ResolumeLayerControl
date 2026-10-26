#pragma once
#include <tinyxml2.h>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace control {
struct ServerSettings { bool enabled; unsigned short port; std::string host; };
inline bool ValidListenAddress(const std::string& address) {
    unsigned octets[4]{};
    size_t begin = 0;
    for (unsigned i = 0; i < 4; ++i) {
        const auto end = address.find('.', begin);
        if ((i < 3) != (end != std::string::npos)) return false;
        const auto part = address.substr(begin, end == std::string::npos ? end : end - begin);
        if (part.empty() || part.size() > 3 || part.find_first_not_of("0123456789") != std::string::npos ||
            (part.size() > 1 && part[0] == '0')) return false;
        octets[i] = static_cast<unsigned>(std::stoul(part));
        if (octets[i] > 255) return false;
        begin = end + 1;
    }
    return address == "0.0.0.0" || (octets[0] > 0 && octets[0] < 224);
}
inline void ValidateSettingsTree(const tinyxml2::XMLNode& node, unsigned depth = 0) {
    if (depth > 16 || node.ToUnknown()) throw std::runtime_error("unsupported server.xml structure/depth");
    for (auto* child = node.FirstChild(); child; child = child->NextSibling())
        ValidateSettingsTree(*child, depth + 1);
}
inline std::filesystem::path ServerSettingsPath(const std::filesystem::path& documents) {
    return documents / "Resolume Arena" / "Preferences" / "server.xml";
}
inline ServerSettings ReadServerSettings(const std::filesystem::path& file) {
    std::ifstream input(file, std::ios::binary);
    if (!input) throw std::runtime_error("cannot open Resolume server.xml");
    std::string xml(65537, '\0');
    input.read(xml.data(), static_cast<std::streamsize>(xml.size()));
    xml.resize(static_cast<size_t>(input.gcount()));
    if (input.bad() || xml.empty() || xml.size() > 65536 || xml.find('\0') != std::string::npos)
        throw std::runtime_error("invalid server.xml size/read/encoding (limit 64 KiB, UTF-8 required)");
    tinyxml2::XMLDocument document;
    if (document.Parse(xml.data(), xml.size()) != tinyxml2::XML_SUCCESS)
        throw std::runtime_error("malformed server.xml");
    ValidateSettingsTree(document);
    // TinyXML-2 accepts fragments: require one root and no DTD/unknown declaration.
    const auto* root = document.RootElement();
    if (!root || std::string(root->Name()) != "ServerController")
        throw std::runtime_error("unexpected server.xml root");
    for (auto* node = document.FirstChild(); node; node = node->NextSibling()) {
        if ((node->ToElement() && node != root) || node->ToUnknown() || node->ToText())
            throw std::runtime_error("invalid server.xml document structure");
    }
    auto attribute = [root](const char* name) {
        const auto* value = root->Attribute(name);
        if (!value) throw std::runtime_error("missing server.xml attribute");
        return std::string(value);
    };
    if (root->Attribute("xmlns")) throw std::runtime_error("unsupported server.xml namespace");
    const auto enabled = attribute("enabled"), port = attribute("port"), address = attribute("address");
    if (enabled != "0" && enabled != "1") throw std::runtime_error("invalid Webserver enabled flag");
    if (port.empty() || port.size() > 5 || port.find_first_not_of("0123456789") != std::string::npos)
        throw std::runtime_error("invalid Webserver port");
    const auto number = std::stoul(port);
    if (number < 1 || number > 65535) throw std::runtime_error("Webserver port out of range");
    if (!ValidListenAddress(address))
        throw std::runtime_error("Webserver listen address must be a unicast IPv4 address or 0.0.0.0");
    return {enabled == "1", static_cast<unsigned short>(number),
            address == "0.0.0.0" ? "127.0.0.1" : address};
}
}
