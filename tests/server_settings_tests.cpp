#include "../source/control/ServerSettings.h"
#include <iostream>
#include "../source/control/Platform.h"

int main() {
    const auto file = std::filesystem::temp_directory_path() / ("rlc-settings-" + control::NewInstanceKey() + ".xml");
    int failures = 0;
    auto check = [&](const std::string& xml, bool valid, unsigned short port = 8080, bool enabled = true) {
        { std::ofstream out(file, std::ios::binary); out << xml; }
        try {
            const auto actual = control::ReadServerSettings(file);
            if (!valid || actual.port != port || actual.enabled != enabled) ++failures;
        } catch (...) { if (valid) ++failures; }
    };
    check("<?xml version='1.0'?><!-- comment --><ServerController address='0.0.0.0' port='8080' enabled='1'><versionInfo name='Resolume Arena'/></ServerController>", true);
    check("<ServerController enabled='0' port='1' address='127.0.0.1'/>", true, 1, false);
    check("<ServerController enabled='1' port='65535' address='127.0.0.1'/>", true, 65535);
    for (const auto* port : {"0", "65536", "-1", "8080x", " 8080", "1.5", "", "99999999999999999"})
        check(std::string("<ServerController enabled='1' address='0.0.0.0' port='") + port + "'/>", false);
    check("<ServerController enabled='true' port='8080' address='0.0.0.0'/>", false);
    check("<ServerController enabled='1' port='8080'/>", false);
    check("<ServerController enabled='1' port='8080' address='192.168.1.2'/>", false);
    check("<Other enabled='1' port='8080' address='0.0.0.0'/>", false);
    check("<ServerController enabled='1' port='8080' address='0.0.0.0'>", false);
    check("<ServerController enabled='1' port='8080' address='0.0.0.0'/><Other/>", false);
    check("<ServerController enabled='1' port='8080' port='9000' address='0.0.0.0'/>", false);
    check("<!DOCTYPE ServerController [<!ENTITY port '8080'>]><ServerController enabled='1' port='&port;' address='0.0.0.0'/>", false);
    check(std::string(65537, ' '), false);
    check("", false);
    check("<ServerController xmlns='other' enabled='1' port='8080' address='0.0.0.0'/>", false);
    std::string deep = "<ServerController enabled='1' port='8080' address='0.0.0.0'>";
    for (int i = 0; i < 17; ++i) deep += "<child>";
    for (int i = 0; i < 17; ++i) deep += "</child>";
    check(deep + "</ServerController>", false);
    std::filesystem::remove(file);
    try { control::ReadServerSettings(file); ++failures; } catch (...) {}
    const auto documents = std::filesystem::path(L"D:/Moved Documents");
    if (control::ServerSettingsPath(documents) !=
        documents / L"Resolume Arena/Preferences/server.xml") ++failures;

    std::cout << "server settings failures: " << failures << '\n';
    return failures ? 1 : 0;
}
