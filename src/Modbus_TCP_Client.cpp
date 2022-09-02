/*
 * Copyright (C) 2021-2022 Nikolas Koesling <nikolas@koesling.info>.
 * This program is free software. You can redistribute it and/or modify it under the terms of the MIT License.
 */

#include "Modbus_TCP_Client.hpp"

#include <algorithm>
#include <arpa/inet.h>
#include <cstring>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sstream>
#include <stdexcept>
#include <sys/socket.h>
#include <system_error>
#include <unistd.h>

#include <iostream>

namespace Modbus {
namespace TCP {

static constexpr int MAX_REGS = 0x10000;

Client::Client(const std::string &ip, unsigned short port, modbus_mapping_t *mapping, std::size_t tcp_timeout) {
    // create modbus object
    modbus = modbus_new_tcp(ip.c_str(), static_cast<int>(port));
    if (modbus == nullptr) {
        const std::string error_msg = modbus_strerror(errno);
        throw std::runtime_error("failed to create modbus instance: " + error_msg);
    }

    modbus_mapping_t *mb_mapping;

    if (mapping == nullptr) {
        // create new mapping with the maximum number of registers
        mb_mapping = modbus_mapping_new(MAX_REGS, MAX_REGS, MAX_REGS, MAX_REGS);
        if (mb_mapping == nullptr) {
            const std::string error_msg = modbus_strerror(errno);
            modbus_free(modbus);
            throw std::runtime_error("failed to allocate memory: " + error_msg);
        }
        delete_mapping = mapping;
    } else {
        // use the provided mapping object
        mb_mapping     = mapping;
        delete_mapping = nullptr;
    }

    // use mapping for all client ids
    for (std::size_t i = 0; i < MAX_CLIENT_IDS; ++i) {
        this->mappings[i] = mapping;
    }

    listen();

#ifdef OS_LINUX
    if (tcp_timeout) set_tcp_timeout(tcp_timeout);
#else
    static_cast<void>(tcp_timeout);
#endif
}

Client::Client(const std::string &ip, unsigned short port, modbus_mapping_t **mappings, std::size_t tcp_timeout) {
    // create modbus object
    modbus = modbus_new_tcp(ip.c_str(), static_cast<int>(port));
    if (modbus == nullptr) {
        const std::string error_msg = modbus_strerror(errno);
        throw std::runtime_error("failed to create modbus instance: " + error_msg);
    }

    delete_mapping = nullptr;

    for (std::size_t i = 0; i < MAX_CLIENT_IDS; ++i) {
        if (mappings[i] == nullptr) {
            if (delete_mapping == nullptr) {
                delete_mapping = modbus_mapping_new(MAX_REGS, MAX_REGS, MAX_REGS, MAX_REGS);

                if (delete_mapping == nullptr) {
                    const std::string error_msg = modbus_strerror(errno);
                    modbus_free(modbus);
                    throw std::runtime_error("failed to allocate memory: " + error_msg);
                }
            }
            this->mappings[i] = delete_mapping;
        } else {
            this->mappings[i] = mappings[i];
        }
    }

    listen();

#ifdef OS_LINUX
    if (tcp_timeout) set_tcp_timeout(tcp_timeout);
#else
    static_cast<void>(tcp_timeout);
#endif
}

void Client::listen() {
    // create tcp socket
    socket = modbus_tcp_listen(modbus, 1);
    if (socket == -1) {
        const std::string error_msg = modbus_strerror(errno);
        throw std::runtime_error("failed to create tcp socket: " + error_msg);
    }

    // set socket options
    // enable socket keepalive (--> fail if connection partner is not reachable)
    int keepalive = 1;
    int tmp       = setsockopt(socket, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive));
    if (tmp != 0) {
        throw std::system_error(errno, std::generic_category(), "Failed to set socket option SO_KEEPALIVE");
    }
}

#ifdef OS_LINUX
void Client::set_tcp_timeout(std::size_t tcp_timeout) {
    // set user timeout (~= timeout for tcp connection)
    unsigned user_timeout = static_cast<unsigned>(tcp_timeout) * 1000;
    int      tmp          = setsockopt(socket, IPPROTO_TCP, TCP_USER_TIMEOUT, &user_timeout, sizeof(tcp_timeout));
    if (tmp != 0) {
        throw std::system_error(errno, std::generic_category(), "Failed to set socket option TCP_USER_TIMEOUT");
    }

    // start sending keepalive request after one second without request
    unsigned keepidle = 1;
    tmp               = setsockopt(socket, IPPROTO_TCP, TCP_KEEPIDLE, &keepidle, sizeof(keepidle));
    if (tmp != 0) {
        throw std::system_error(errno, std::generic_category(), "Failed to set socket option TCP_KEEPIDLE");
    }

    // send up to 5 keepalive requests during the timeout time, but not more than one per second
    unsigned keepintvl = std::max(static_cast<unsigned>(tcp_timeout / 5), 1u);
    tmp                = setsockopt(socket, IPPROTO_TCP, TCP_KEEPINTVL, &keepintvl, sizeof(keepintvl));
    if (tmp != 0) {
        throw std::system_error(errno, std::generic_category(), "Failed to set socket option TCP_KEEPINTVL");
    }

    // 5 keepalive requests if the timeout time is >= 5s; else send one request each second
    unsigned keepcnt = std::min(static_cast<unsigned>(tcp_timeout), 5u);
    tmp              = setsockopt(socket, IPPROTO_TCP, TCP_KEEPCNT, &keepcnt, sizeof(keepcnt));
    if (tmp != 0) {
        throw std::system_error(errno, std::generic_category(), "Failed to set socket option TCP_KEEPCNT");
    }
}
#endif


Client::~Client() {
    if (modbus != nullptr) {
        modbus_close(modbus);
        modbus_free(modbus);
    }
    if (delete_mapping) modbus_mapping_free(delete_mapping);
    if (socket != -1) { close(socket); }
}

void Client::set_debug(bool debug) {
    if (modbus_set_debug(modbus, debug)) {
        const std::string error_msg = modbus_strerror(errno);
        throw std::runtime_error("failed to enable modbus debugging mode: " + error_msg);
    }
}

std::string Client::connect_client() {
    int tmp = modbus_tcp_accept(modbus, &socket);
    if (tmp < 0) {
        const std::string error_msg = modbus_strerror(errno);
        throw std::runtime_error("modbus_tcp_accept failed: " + error_msg);
    }

    struct sockaddr_in peer_addr;
    socklen_t          len = sizeof(peer_addr);
    tmp = getpeername(modbus_get_socket(modbus), reinterpret_cast<struct sockaddr *>(&peer_addr), &len);

    if (tmp < 0) {
        const std::string error_msg = modbus_strerror(errno);
        throw std::runtime_error("getpeername failed: " + error_msg);
    }

    char buffer[INET_ADDRSTRLEN];
    inet_ntop(peer_addr.sin_family, &peer_addr.sin_addr, buffer, sizeof(buffer));

    std::ostringstream sstr;
    sstr << buffer << ':' << htons(peer_addr.sin_port);

    return sstr.str();
}

bool Client::handle_request() {
    // receive modbus request
    uint8_t query[MODBUS_TCP_MAX_ADU_LENGTH];
    int     rc = modbus_receive(modbus, query);

    if (rc > 0) {
        const auto CLIENT_ID = query[6];

        // get mapping
        auto mapping = mappings[CLIENT_ID];

        // handle request
        int ret = modbus_reply(modbus, query, rc, mapping);
        if (ret == -1) {
            const std::string error_msg = modbus_strerror(errno);
            throw std::runtime_error("modbus_reply failed: " + error_msg + ' ' + std::to_string(errno));
        }
    } else if (rc == -1) {
        if (errno == ECONNRESET) return true;

        const std::string error_msg = modbus_strerror(errno);
        throw std::runtime_error("modbus_receive failed: " + error_msg + ' ' + std::to_string(errno));
    }

    return false;
}

struct timeout_t {
    uint32_t sec;
    uint32_t usec;
};

static inline timeout_t double_to_timeout_t(double timeout) {
    timeout_t ret {};

    ret.sec = static_cast<uint32_t>(timeout);

    double fractional = timeout - static_cast<double>(ret.sec);
    ret.usec          = static_cast<uint32_t>(fractional * 1000.0 * 1000.0);

    return ret;
}

void Client::set_byte_timeout(double timeout) {
    const auto T   = double_to_timeout_t(timeout);
    auto       ret = modbus_set_byte_timeout(modbus, T.sec, T.usec);

    if (ret != 0) {
        const std::string error_msg = modbus_strerror(errno);
        throw std::runtime_error("modbus_receive failed: " + error_msg + ' ' + std::to_string(errno));
    }
}

void Client::set_response_timeout(double timeout) {
    const auto T   = double_to_timeout_t(timeout);
    auto       ret = modbus_set_response_timeout(modbus, T.sec, T.usec);

    if (ret != 0) {
        const std::string error_msg = modbus_strerror(errno);
        throw std::runtime_error("modbus_receive failed: " + error_msg + ' ' + std::to_string(errno));
    }
}

double Client::get_byte_timeout() {
    timeout_t timeout {};

    auto ret = modbus_get_byte_timeout(modbus, &timeout.sec, &timeout.usec);

    if (ret != 0) {
        const std::string error_msg = modbus_strerror(errno);
        throw std::runtime_error("modbus_receive failed: " + error_msg + ' ' + std::to_string(errno));
    }

    return static_cast<double>(timeout.sec) + (static_cast<double>(timeout.usec) / (1000.0 * 1000.0));
}

double Client::get_response_timeout() {
    timeout_t timeout {};

    auto ret = modbus_get_response_timeout(modbus, &timeout.sec, &timeout.usec);

    if (ret != 0) {
        const std::string error_msg = modbus_strerror(errno);
        throw std::runtime_error("modbus_receive failed: " + error_msg + ' ' + std::to_string(errno));
    }

    return static_cast<double>(timeout.sec) + (static_cast<double>(timeout.usec) / (1000.0 * 1000.0));
}

}  // namespace TCP
}  // namespace Modbus
