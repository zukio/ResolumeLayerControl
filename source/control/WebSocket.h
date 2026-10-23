#pragma once
#include <memory>
#include <string>
namespace control {
class WebSocket {
public:
    WebSocket();
    ~WebSocket();
    void Connect(unsigned short port);
    bool Receive(std::string& message);
    bool Send(const std::string& message);
    void Cancel() noexcept;
    void Shutdown() noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
}
