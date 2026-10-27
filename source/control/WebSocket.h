#pragma once
#include <memory>
#include <string>
#include <mutex>
#include <cstdint>
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
    struct Statistics {
        std::uint64_t received = 0, compositionsReplaced = 0, positionsIgnored = 0;
        std::size_t peakMessages = 0, peakBytes = 0, largestMessage = 0;
    };
    Statistics Stats() const;
private:
    struct Impl;
    std::shared_ptr<Impl> impl;
    mutable std::mutex lifetime;
    bool cancelled = false;
};
}
