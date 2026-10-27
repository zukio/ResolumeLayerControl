#include "WebSocket.h"
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#include <atomic>
#include <deque>
#include <stdexcept>
#include <string>
#include <chrono>
#include <condition_variable>
#include <thread>
#include <nlohmann/json.hpp>

namespace control {
// Validate the full JSON without constructing its large nested DOM. Only a
// top-level layers array identifies a replaceable full Composition snapshot.
struct MessageKind { bool snapshot = false, position = false; };
static MessageKind Classify(const std::string& raw) {
    bool layers = false, snapshot = false, hasError = false;
    auto parsed = nlohmann::json::parse(raw, [&](int depth, nlohmann::json::parse_event_t event, nlohmann::json& value) {
        if (depth == 1 && event == nlohmann::json::parse_event_t::key) {
            layers = value == "layers";
            if (value == "error") hasError = true;
        }
        if (depth == 1 && event == nlohmann::json::parse_event_t::array_start && layers) snapshot = true;
        return depth <= 1;
    }, false);
    if (parsed.is_discarded() || !parsed.is_object()) return {};
    const auto path = parsed.find("path"), type = parsed.find("type");
    bool position = false;
    if (!snapshot && !hasError && path != parsed.end() && path->is_string() &&
        type != parsed.end() && *type == "parameter_update") {
        const auto& text = path->get_ref<const std::string&>();
        const std::string suffix = "/transport/position";
        position = text.size() >= suffix.size() && text.compare(text.size()-suffix.size(), suffix.size(), suffix) == 0;
    }
    return {snapshot, position};
}
// I/O keeps receiving/coalescing while Runtime evaluates the previous snapshot.
struct WebSocket::Impl {
    using Client = websocketpp::client<websocketpp::config::asio_client>;
public:
    Impl() {
        client.clear_access_channels(websocketpp::log::alevel::all);
        client.clear_error_channels(websocketpp::log::elevel::all);
        client.init_asio();
        // Captured Clip metadata expanded to 30x30 with Group copies exceeds
        // 16 MiB. One snapshot has a separate wire cap from response traffic.
        client.set_max_message_size(64 * 1024 * 1024);
        client.set_open_handshake_timeout(3000);
        client.set_close_handshake_timeout(1000);
        client.set_open_handler([this](auto) { opened = true; });
        client.set_fail_handler([this](auto) { End("WebSocket connection failed"); });
        client.set_close_handler([this](auto) { End(""); });
        client.set_message_handler([this](auto, Client::message_ptr message) {
            nativeSendBytes.store(connection->get_buffered_amount());
            const auto& payload = message->get_payload();
            const auto kind = message->get_opcode() == websocketpp::frame::opcode::text ? Classify(payload) : MessageKind{};
            const bool snapshot = kind.snapshot;
            std::lock_guard<std::mutex> lock(queueMutex);
            if (message->get_opcode() != websocketpp::frame::opcode::text) {
                failure = "non-text WebSocket message"; ended = true; available.notify_all(); client.stop(); return;
            }
            ++stats.received;
            stats.largestMessage = std::max(stats.largestMessage, payload.size());
            // Playback position is neither a controlled property nor a sync
            // trigger. Error-bearing updates are deliberately retained.
            if (kind.position) { ++stats.positionsIgnored; return; }
            if (snapshot) {
                // Retain all responses/errors and their order. Append the newest
                // snapshot after them, never execute an obsolete Active state.
                for (auto it = messages.begin(); it != messages.end();) {
                    if (it->snapshot) {
                        queuedBytes -= it->raw.size(); it = messages.erase(it);
                        ++stats.compositionsReplaced;
                    } else ++it;
                }
            }
            std::size_t responseBytes = 0, responseCount = 0;
            for (const auto& queued : messages) if (!queued.snapshot) { ++responseCount; responseBytes += queued.raw.size(); }
            if (!snapshot && (responseCount >= 32 || responseBytes + payload.size() > 32 * 1024 * 1024)) {
                failure = "WebSocket receive queue limit: messages=" + std::to_string(messages.size()) +
                    " bytes=" + std::to_string(queuedBytes) + " incoming=" + std::to_string(payload.size()) +
                    " composition=" + std::to_string(snapshot); ended = true; available.notify_all(); client.stop(); return;
            }
            messages.push_back({payload, snapshot});
            queuedBytes += payload.size();
            stats.peakMessages = std::max(stats.peakMessages, messages.size());
            stats.peakBytes = std::max(stats.peakBytes, queuedBytes);
            available.notify_one();
        });
    }
    ~Impl() { Shutdown(); }
    void Connect(unsigned short port, const std::string& host) {
        if (cancelled.load()) throw std::runtime_error("WebSocket cancelled");
        // A configured listen address must belong to this machine. Binding a
        // temporary socket checks this without connecting to a different host.
        const auto address = asio::ip::make_address_v4(host);
        if (address.is_unspecified() || address.is_multicast() || address.to_uint() == 0xffffffff)
            throw std::runtime_error("invalid local WebSocket address");
        asio::io_context probeContext;
        asio::ip::tcp::socket probe(probeContext);
        probe.open(asio::ip::tcp::v4());
        asio::error_code bindError;
        probe.bind(asio::ip::tcp::endpoint(address, 0), bindError);
        if (bindError) throw std::runtime_error("Webserver listen address is not available locally");
        probe.close();
        websocketpp::lib::error_code error;
        connection = client.get_connection("ws://" + host + ":" + std::to_string(port) + "/api/v1", error);
        if (error) throw std::runtime_error("cannot create WebSocket connection");
        client.connect(connection);
        while (!opened && !ended && !cancelled.load()) {
            if (!client.run_one()) break;
        }
        if (!opened || cancelled.load()) throw std::runtime_error("WebSocket connection failed or cancelled");
        ioThread = std::thread([this] {
            try { client.run(); End(""); }
            catch (const std::exception& e) { End(e.what()); }
            catch (...) { End("WebSocket I/O exception"); }
        });
    }
    bool Receive(std::string& message) {
        std::unique_lock<std::mutex> lock(queueMutex);
        available.wait(lock, [&] { return !messages.empty() || ended || cancelled.load(); });
        if (!failure.empty()) throw std::runtime_error(failure);
        if (cancelled.load() || ended) return false;
        message = std::move(messages.front().raw); messages.pop_front();
        queuedBytes -= message.size();
        return true;
    }
    bool Send(const std::string& message) {
        if (cancelled.load() || ended || !opened || message.size() > 1024 * 1024) return false;
        const auto reserved = postedSendBytes.fetch_add(message.size()) + message.size();
        if (reserved + nativeSendBytes.load() > 1024 * 1024) {
            postedSendBytes.fetch_sub(message.size()); return false;
        }
        try {
            // All connection access stays on the I/O thread: this pinned
            // WebSocket++ version does not lock get_buffered_amount().
            client.get_io_service().post([this, message] {
                websocketpp::lib::error_code error;
                if (!cancelled.load() && !ended) client.send(connection, message, websocketpp::frame::opcode::text, error);
                nativeSendBytes.store(connection->get_buffered_amount());
                postedSendBytes.fetch_sub(message.size());
                if (error) { End("WebSocket send failed: " + error.message()); client.stop(); }
            });
            return true; // queue acceptance, not acknowledgement/readback
        } catch (...) { postedSendBytes.fetch_sub(message.size()); return false; }
    }
    void Cancel() noexcept {
        { std::lock_guard<std::mutex> lock(queueMutex); cancelled.store(true); }
        try { client.stop(); } catch (...) {}
        available.notify_all();
    }
    // Worker only (or destructor after the worker has joined).
    void Shutdown() noexcept {
        Cancel();
        if (ioThread.joinable()) ioThread.join();
        if (connection) {
            websocketpp::lib::asio::error_code error;
            connection->get_socket().cancel(error);
            connection->get_socket().close(error);
        }
    }
    Statistics GetStats() const { std::lock_guard<std::mutex> lock(queueMutex); return stats; }
private:
    void End(const std::string& reason) {
        std::lock_guard<std::mutex> lock(queueMutex);
        ended = true;
        if (!reason.empty()) failure = reason;
        available.notify_all();
    }
    Client client;
    Client::connection_ptr connection;
    std::atomic<bool> cancelled{false};
    std::atomic<bool> opened{false}, ended{false};
    std::atomic<std::size_t> postedSendBytes{0}, nativeSendBytes{0};
    std::string failure;
    struct Message { std::string raw; bool snapshot; };
    std::deque<Message> messages;
    size_t queuedBytes = 0;
    Statistics stats;
    mutable std::mutex queueMutex;
    std::condition_variable available;
    std::thread ioThread;
};
WebSocket::WebSocket() : impl(std::make_shared<Impl>()) {}
WebSocket::~WebSocket() = default;
void WebSocket::Connect(unsigned short port, const std::string& host) {
    std::shared_ptr<Impl> next;
    {
        std::lock_guard<std::mutex> lock(lifetime);
        if (cancelled) throw std::runtime_error("WebSocket cancelled");
        impl = next = std::make_shared<Impl>();
    }
    next->Connect(port, host);
}
bool WebSocket::Receive(std::string& message) { return impl->Receive(message); }
bool WebSocket::Send(const std::string& message) { return impl->Send(message); }
void WebSocket::Cancel() noexcept { std::lock_guard<std::mutex> lock(lifetime); cancelled = true; impl->Cancel(); }
void WebSocket::Shutdown() noexcept { impl->Shutdown(); }
WebSocket::Statistics WebSocket::Stats() const { return impl->GetStats(); }

}
