#pragma once
#include <memory>
#include <string>
namespace control {
class WebSocket {
public:
    WebSocket();
    ~WebSocket();
    void Connect(unsigned short port, const std::string& host = "127.0.0.1");
    bool Receive(std::string& message);
    bool Send(const std::string& message);
    void Cancel() noexcept;
    void Shutdown() noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
}
