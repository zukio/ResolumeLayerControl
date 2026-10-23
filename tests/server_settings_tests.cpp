#include "../source/control/ServerSettings.h"
#include <iostream>

int main() {
    wchar_t temp[MAX_PATH], name[MAX_PATH];
    if (!GetTempPathW(MAX_PATH, temp) || !GetTempFileNameW(temp, L"rlc", 0, name)) return 1;
    const std::filesystem::path file(name);
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
    std::filesystem::remove(file);
    try { control::ReadServerSettings(file); ++failures; } catch (...) {}
    const auto documents = std::filesystem::path(L"D:/Moved Documents");
    if (control::ServerSettingsPath(documents, L"C:/Program Files/Resolume Arena/ARENA.EXE") !=
        documents / L"Resolume Arena/Preferences/server.xml") ++failures;
    try { control::ServerSettingsPath(documents, L"Avenue.exe"); ++failures; } catch (...) {}
    std::cout << "server settings failures: " << failures << '\n';
    return failures ? 1 : 0;
}
