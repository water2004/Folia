#pragma once
#include <winsock.h>
namespace completion_tests {
struct Server {
    SOCKET listener = INVALID_SOCKET;
    std::atomic<SOCKET> client = INVALID_SOCKET;
    std::thread thread;
    std::string baseUrl, request;
    std::vector<std::string> requests;
    std::atomic<bool> received = false;
    std::atomic<int> receivedRequests = 0;
    Server(std::string body, int status = 200, bool hold = false, std::string contentType = "text/event-stream",
        std::chrono::milliseconds fragmentDelay = {}, std::size_t fragmentBytes = 37, std::vector<std::string> followingPages = {}) {
        WSADATA data{};
        if (WSAStartup(MAKEWORD(2, 2), &data)) throw std::runtime_error("test socket init");
        listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        sockaddr_in address{}; address.sin_family = AF_INET; address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) || listen(listener, 1)) throw std::runtime_error("test socket bind");
        int length = sizeof(address); getsockname(listener, reinterpret_cast<sockaddr*>(&address), &length);
        baseUrl = "http://127.0.0.1:" + std::to_string(ntohs(address.sin_port));
        followingPages.insert(followingPages.begin(), std::move(body));
        thread = std::thread([this, pages = std::move(followingPages), status, hold, contentType = std::move(contentType), fragmentDelay, fragmentBytes] {
          for (auto const& body : pages) {
            auto socket = accept(listener, nullptr, nullptr); client = socket;
            if (socket == INVALID_SOCKET) return;
            request.clear();
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
            requests.push_back(request); received = true; ++receivedRequests;
            const auto headers = "HTTP/1.1 " + std::to_string(status) + " OK\r\nContent-Type: " + contentType + "\r\nContent-Length: "
                + std::to_string(hold ? 10000 : body.size()) + "\r\nConnection: close\r\n\r\n";
            auto sendAll = [&](std::string_view bytes) {
                while (!bytes.empty()) {
                    auto sent = send(socket, bytes.data(), static_cast<int>(bytes.size()), 0);
                    if (sent <= 0) return false;
                    bytes.remove_prefix(sent);
                }
                return true;
            };
            if (sendAll(headers)) for (std::size_t at = 0; at < body.size();) {
                auto count = (std::min)(fragmentBytes, body.size() - at);
                if (!sendAll(std::string_view(body).substr(at, count))) break;
                at += count;
                if (fragmentDelay.count() && at < body.size()) std::this_thread::sleep_for(fragmentDelay);
            }
            if (hold) (void)recv(socket, buffer.data(), 1, 0);
            if (client.exchange(INVALID_SOCKET) == socket) closesocket(socket);
          }
        });
    }
    ~Server() {
        closesocket(listener);
        if (auto socket = client.exchange(INVALID_SOCKET); socket != INVALID_SOCKET) { shutdown(socket, 2); closesocket(socket); }
        if (thread.joinable()) thread.join(); WSACleanup();
    }
};
}
