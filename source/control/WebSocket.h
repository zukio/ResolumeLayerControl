#pragma once
#define NOMINMAX
#include <windows.h>
#include <winhttp.h>
#include <mutex>
#include <string>
#include <stdexcept>

namespace control {
// Windows platform adapter. One receive/send worker; cancellation from the owner.
class WebSocket {
public:
    ~WebSocket() { Cancel(); }
    void Connect(unsigned short port) {
        std::lock_guard<std::mutex> lock(mutex);
        session = WinHttpOpen(L"LayerControl/0.3", WINHTTP_ACCESS_TYPE_NO_PROXY,
                              WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!session) throw std::runtime_error("WinHttpOpen failed");
        WinHttpSetTimeouts(session, 1000, 1000, 1000, 1000);
        connection = WinHttpConnect(session, L"127.0.0.1", port, 0);
        if (!connection) throw std::runtime_error("WinHttpConnect failed");
        request = WinHttpOpenRequest(connection, L"GET", L"/api/v1", nullptr,
                                     WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
        if (!request || !WinHttpSetOption(request, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, nullptr, 0) ||
            !WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, nullptr, 0, 0, 0) ||
            !WinHttpReceiveResponse(request, nullptr)) throw std::runtime_error("WebSocket upgrade failed");
        DWORD status = 0, size = sizeof(status);
        if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                 nullptr, &status, &size, nullptr) || status != 101)
            throw std::runtime_error("server did not upgrade WebSocket");
        socket = WinHttpWebSocketCompleteUpgrade(request, 0);
        if (!socket) throw std::runtime_error("WebSocket completion failed");
        WinHttpCloseHandle(request); request = nullptr;
    }
    bool Receive(std::string& message) {
        message.clear();
        for (;;) {
            HINTERNET handle;
            { std::lock_guard<std::mutex> lock(mutex); handle = socket; }
            if (!handle) return false;
            char buffer[16384]; DWORD count = 0;
            WINHTTP_WEB_SOCKET_BUFFER_TYPE type;
            const DWORD error = WinHttpWebSocketReceive(handle, buffer, sizeof(buffer), &count, &type);
            if (error != NO_ERROR || type == WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE) return false;
            if (type != WINHTTP_WEB_SOCKET_UTF8_FRAGMENT_BUFFER_TYPE && type != WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE)
                throw std::runtime_error("non-text WebSocket message");
            if (message.size() + count > 16 * 1024 * 1024) throw std::runtime_error("Composition exceeds 16 MiB limit");
            message.append(buffer, count);
            if (type == WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE) return true;
        }
    }
    bool Send(const std::string& message) {
        std::lock_guard<std::mutex> lock(mutex);
        return socket && WinHttpWebSocketSend(socket, WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE,
                    const_cast<char*>(message.data()), static_cast<DWORD>(message.size())) == NO_ERROR;
    }
    void Cancel() noexcept {
        std::lock_guard<std::mutex> lock(mutex);
        // Closing the handle cancels an outstanding synchronous receive.
        for (auto h : { &socket, &request, &connection, &session })
            if (*h) { WinHttpCloseHandle(*h); *h = nullptr; }
    }
private:
    std::mutex mutex;
    HINTERNET session = nullptr, connection = nullptr, request = nullptr, socket = nullptr;
};
}
