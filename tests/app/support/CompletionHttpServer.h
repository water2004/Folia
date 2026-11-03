#pragma once
#include <winsock.h>
namespace completion_tests {
struct Server {
    SOCKET listener = INVALID_SOCKET;
    std::atomic<SOCKET> client = INVALID_SOCKET;
    std::thread thread;
    std::string baseUrl, request;
    std::atomic<bool> received = false;
    Server(std::string body, int status = 200, bool hold = false, std::string contentType = "text/event-stream") {
        WSADATA data{};
        if (WSAStartup(MAKEWORD(2, 2), &data)) throw std::runtime_error("test socket init");
        listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        sockaddr_in address{}; address.sin_family = AF_INET; address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) || listen(listener, 1)) throw std::runtime_error("test socket bind");
        int length = sizeof(address); getsockname(listener, reinterpret_cast<sockaddr*>(&address), &length);
        baseUrl = "http://127.0.0.1:" + std::to_string(ntohs(address.sin_port));
        thread = std::thread([this, body = std::move(body), status, hold, contentType = std::move(contentType)] {
            auto socket = accept(listener, nullptr, nullptr); client = socket;
            if (socket == INVALID_SOCKET) return;
            int timeout = 2000;
            setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char*>(&timeout), sizeof(timeout));
            setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<char*>(&timeout), sizeof(timeout));
            std::array<char, 4096> buffer;
            std::size_t bodySize = 0;
            while (request.size() < 64 * 1024) {
                auto count = recv(socket, buffer.data(), static_cast<int>(buffer.size()), 0); if (count <= 0) break;
                request.append(buffer.data(), count);
                auto end = request.find("\r\n\r\n");
                if (end != std::string::npos) {
                    auto header = request.find("Content-Length:");
                    if (header != std::string::npos) bodySize = std::stoul(request.substr(header + 15));
                    if (request.size() >= end + 4 + bodySize) break;
                }
            }
            received = true;
            auto response = "HTTP/1.1 " + std::to_string(status) + " OK\r\nContent-Type: " + contentType + "\r\nContent-Length: "
                + std::to_string(hold ? 10000 : body.size()) + "\r\nConnection: close\r\n\r\n" + body;
            for (std::size_t at = 0; at < response.size();) {
                auto sent = send(socket, response.data() + at, static_cast<int>((std::min)(std::size_t{37}, response.size() - at)), 0);
                if (sent <= 0) break; at += sent;
            }
            if (hold) (void)recv(socket, buffer.data(), 1, 0);
            if (client.exchange(INVALID_SOCKET) == socket) closesocket(socket);
        });
    }
    ~Server() {
        closesocket(listener);
        if (auto socket = client.exchange(INVALID_SOCKET); socket != INVALID_SOCKET) { shutdown(socket, 2); closesocket(socket); }
        if (thread.joinable()) thread.join(); WSACleanup();
    }
};
}
