#pragma once
// Diagnostic transport: fixed OSC address, no Resolume control commands.
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif
#include <array>
#include <cstring>
#include <atomic>
#include <chrono>

#include <cstdint>
#include <cstdlib>
#include <fstream>

#include <thread>
#include <string>
#include <iomanip>

class ProbeWorker {
    struct Event { const char* name; bool send; uint32_t sequence; int64_t time; int64_t wallMs; double a, b, c; char text[160]; };
    struct Slot { std::atomic<size_t> turn{0}; Event event{}; };
    static const size_t Capacity = 4096;
    std::array<Slot, Capacity> queue{};
    std::atomic<size_t> enqueue{0};
    size_t dequeue = 0; // worker only
    std::atomic<bool> stop{false};
    std::atomic<unsigned> dropped{0};
    std::thread worker;
    bool Pop(Event& event) noexcept {
        Slot& slot = queue[dequeue % Capacity];
        if (slot.turn.load(std::memory_order_acquire) != dequeue + 1) return false;
        event = slot.event;
        slot.turn.store(dequeue + Capacity, std::memory_order_release);
        ++dequeue;
        return true;
    }
    void Run() noexcept {
        try {
            // Explicit environment configuration; no assumed Resolume port.
            const char* path = std::getenv("RLC_PROBE_LOG");
            const char* portText = std::getenv("RLC_PROBE_PORT");
            char* end = nullptr;
            long port = portText ? std::strtol(portText, &end, 10) : 0;
            if (!portText || end == portText || *end || port < 1 || port > 65535) port = 0;
            std::ofstream log;
            if (path) log.open(path, std::ios::app);
#ifdef _WIN32
            WSADATA data{};
            const bool ready = WSAStartup(MAKEWORD(2,2), &data) == 0;
            SOCKET sock = ready ? socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP) : INVALID_SOCKET;
#else
            int sock = socket(AF_INET, SOCK_DGRAM, 0);
#endif
            sockaddr_in target{};
            target.sin_family = AF_INET;
            target.sin_port = htons(static_cast<uint16_t>(port));
            target.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            auto lastFlush = std::chrono::steady_clock::now();
            for (;;) {
                Event event{};
                if (!Pop(event)) {
                    if (stop.load()) break;
                    if (log) log.flush();
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                    continue;
                }
                int result = -1;
                if (event.send && port && !stop.load()) {
                    // /rlc/probe ,i sequence; OSC strings padded to 4 bytes.
                    char packet[20] = {'/','r','l','c','/','p','r','o','b','e',0,0,',','i',0,0};
                    uint32_t value = htonl(event.sequence);
                    std::memcpy(packet + 16, &value, 4);
                    result = static_cast<int>(sendto(sock, packet, sizeof(packet), 0,
                        reinterpret_cast<const sockaddr*>(&target), sizeof(target)));
                }
                if (log) {
                    log << std::setprecision(17) << this << ',' << event.sequence << ',' << event.time << ',' << event.name
                        << ",osc_requested=" << event.send << ",send_bytes=" << result
                        << ",dropped=" << dropped.load()
                        << ",wall_ms=" << event.wallMs << ",a=" << event.a << ",b=" << event.b << ",c=" << event.c
                        << ",text=" << event.text << '\n';
                    auto now = std::chrono::steady_clock::now();
                    if (now - lastFlush >= std::chrono::milliseconds(100)) {
                        log.flush(); lastFlush = now;
                    }
                }
            }
            if (log) {
                log << "worker_stop,dropped=" << dropped.load() << ",discarded=0\n";
                log.flush();
            }
#ifdef _WIN32
            if (sock != INVALID_SOCKET) closesocket(sock);
            if (ready) WSACleanup();
#else
            if (sock >= 0) close(sock);
#endif
        } catch (...) { /* Never propagate through host or thread boundary. */ }
    }
public:
    ProbeWorker() noexcept {
        for (size_t i = 0; i < Capacity; ++i) queue[i].turn.store(i);
        try { worker = std::thread([this]{ Run(); }); } catch (...) { ++dropped; }
    }
    ~ProbeWorker() {
        // Host must finish callbacks before destroying the instance.
        stop.store(true);
        if (worker.joinable()) worker.join();
    }
    void Record(const char* name, bool send = false, double a = 0, double b = 0, double c = 0,
                const char* text = nullptr) noexcept {
        try {
            if (stop.load()) { ++dropped; return; }
            // Bounded multiple-producer ring. Consumer never locks producers out.
            // Bound CAS attempts as well, so rendering cannot wait indefinitely.
            size_t position = enqueue.load(std::memory_order_relaxed);
            Slot* reserved = nullptr;
            for (unsigned attempt = 0; attempt < 64; ++attempt) {
                Slot& slot = queue[position % Capacity];
                const intptr_t difference = static_cast<intptr_t>(slot.turn.load(std::memory_order_acquire))
                    - static_cast<intptr_t>(position);
                if (difference == 0) {
                    if (enqueue.compare_exchange_weak(position, position + 1, std::memory_order_relaxed)) {
                        reserved = &slot; break;
                    }
                } else if (difference < 0) { ++dropped; return; }
                else position = enqueue.load(std::memory_order_relaxed);
            }
            if (!reserved) { ++dropped; return; }
            auto time = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
            auto wallMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            Event& event = reserved->event;
            event = {name, send, static_cast<uint32_t>(position + 1), time, wallMs, a, b, c, {0}};
            if (text) {
                size_t i = 0;
                for (; i + 1 < sizeof(event.text) && text[i]; ++i) {
                    char ch = text[i];
                    event.text[i] = (ch == ',' || ch == '\n' || ch == '\r') ? ' ' : ch;
                }
                event.text[i] = 0;
            }
            reserved->turn.store(position + 1, std::memory_order_release);
        } catch (...) { ++dropped; }
    }
};
