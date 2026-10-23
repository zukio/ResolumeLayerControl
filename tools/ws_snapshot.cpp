// Read-only native transport diagnostic. Never sends set, get, or trigger.
#include "../source/control/WebSocket.h"
#include "../source/control/Composition.h"
#include <fstream>
#include <iostream>
#include <filesystem>
int main(int argc, char** argv) {
    try {
        if (argc != 3) throw std::runtime_error("usage: ws_snapshot <port> <new-output.json>");
        const std::string portText(argv[1]);
        if (portText.empty() || portText.find_first_not_of("0123456789") != std::string::npos) throw std::runtime_error("invalid port");
        const auto port = std::stoul(portText);
        if (port < 1 || port > 65535 || std::filesystem::exists(argv[2])) throw std::runtime_error("invalid port or output already exists");
        control::WebSocket socket;
        socket.Connect(static_cast<unsigned short>(port));
        std::string raw;
        for (unsigned count = 0; count < 3 && socket.Receive(raw); ++count) {
            const auto message = control::Json::parse(raw);
            if (!control::IsComposition(message)) continue;
            std::ofstream output(argv[2]);
            if (!output) throw std::runtime_error("cannot open output");
            output << message.dump(2) << '\n'; output.close();
            if (!output) throw std::runtime_error("cannot write output");
            std::cout << "Composition received: " << message["layers"].size() << " layers, " << raw.size() << " bytes\n";
            return 0;
        }
        throw std::runtime_error("no Composition received");
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
