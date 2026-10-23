#include "../source/control/WebSocket.h"
#include <iostream>
#include <thread>
int main(int argc, char** argv) {
    if (argc != 2) return 2;
    control::WebSocket socket;
    const auto port = static_cast<unsigned short>(std::stoul(argv[1]));
    std::thread worker([&] {
        try {
            socket.Connect(port);
            std::cout << "connected" << std::endl;
            std::string message;
            while (socket.Receive(message)) std::cout << "text " << message.size() << std::endl;
            std::cout << "ended" << std::endl;
        } catch (...) { std::cout << "failed" << std::endl; }
        socket.Shutdown();
    });
    std::string command;
    std::getline(std::cin, command);
    socket.Cancel(); worker.join();
}
