#include "WebSocket.h"
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#include <atomic>
#include <deque>
#include <stdexcept>
#include <string>

namespace control {
// Only the Runtime worker drives Asio; the owner may call Cancel.
struct WebSocket::Impl {
    using Client = websocketpp::client<websocketpp::config::asio_client>;
public:
    Impl() {
        client.clear_access_channels(websocketpp::log::alevel::all);
        client.clear_error_channels(websocketpp::log::elevel::all);
        client.init_asio();
        client.set_max_message_size(16 * 1024 * 1024);
        client.set_open_handshake_timeout(3000);
        client.set_close_handshake_timeout(1000);
        client.set_open_handler([this](auto) { opened = true; });
        client.set_fail_handler([this](auto) { ended = true; failure = "WebSocket connection failed"; });
        client.set_close_handler([this](auto) { ended = true; });
        client.set_message_handler([this](auto, Client::message_ptr message) {
            if (message->get_opcode() != websocketpp::frame::opcode::text) {
                failure = "non-text WebSocket message"; ended = true; return;
            }
            const auto& payload = message->get_payload();
            if (messages.size() >= 32 || queuedBytes + payload.size() > 32 * 1024 * 1024) {
                failure = "WebSocket receive queue limit"; ended = true; return;
            }
            messages.push_back(payload);
            queuedBytes += payload.size();
        });
    }
    ~Impl() { Shutdown(); }
    void Connect(unsigned short port) {
        if (cancelled.load()) throw std::runtime_error("WebSocket cancelled");
        websocketpp::lib::error_code error;
        connection = client.get_connection("ws://127.0.0.1:" + std::to_string(port) + "/api/v1", error);
        if (error) throw std::runtime_error("cannot create WebSocket connection");
        client.connect(connection);
        while (!opened && !ended && !cancelled.load()) {
            if (!client.run_one()) break;
        }
        if (!opened || cancelled.load()) throw std::runtime_error("WebSocket connection failed or cancelled");
    }
    bool Receive(std::string& message) {
        while (messages.empty() && !ended && !cancelled.load()) {
            if (!client.run_one()) { ended = true; break; }
        }
        if (!failure.empty()) throw std::runtime_error(failure);
        if (cancelled.load() || ended) return false;
        message = std::move(messages.front()); messages.pop_front();
        queuedBytes -= message.size();
        return true;
    }
    bool Send(const std::string& message) {
        if (cancelled.load() || ended || !opened || message.size() > 1024 * 1024 ||
            connection->get_buffered_amount() + message.size() > 1024 * 1024) return false;
        websocketpp::lib::error_code error;
        // Success means queued for transport, not server acknowledgement/readback.
        client.send(connection, message, websocketpp::frame::opcode::text, error);
        return !error;
    }
    void Cancel() noexcept {
        cancelled.store(true);
        try { client.stop(); } catch (...) {}
    }
    // Worker only (or destructor after the worker has joined).
    void Shutdown() noexcept {
        Cancel();
        if (connection) {
            websocketpp::lib::asio::error_code error;
            connection->get_socket().cancel(error);
            connection->get_socket().close(error);
        }
    }
private:
    Client client;
    Client::connection_ptr connection;
    std::atomic<bool> cancelled{false};
    bool opened = false, ended = false;
    std::string failure;
    std::deque<std::string> messages;
    size_t queuedBytes = 0;
};
WebSocket::WebSocket() : impl(std::make_unique<Impl>()) {}
WebSocket::~WebSocket() = default;
void WebSocket::Connect(unsigned short port) { impl->Connect(port); }
bool WebSocket::Receive(std::string& message) { return impl->Receive(message); }
bool WebSocket::Send(const std::string& message) { return impl->Send(message); }
void WebSocket::Cancel() noexcept { impl->Cancel(); }
void WebSocket::Shutdown() noexcept { impl->Shutdown(); }

}
