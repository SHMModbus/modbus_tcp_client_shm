/*
 * Copyright (C) 2021-2022 Nikolas Koesling <nikolas@koesling.info>.
 * This program is free software. You can redistribute it and/or modify it under the terms of the MIT License.
 */

#include "Modbus_TCP_Slave.hpp"

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdexcept>
#include <sys/socket.h>
#include <system_error>
#include <unistd.h>

namespace Modbus {
namespace TCP {

static constexpr int MAX_REGS = 0x10000;

Slave::Slave(const std::string &ip, unsigned short port, modbus_mapping_t *mapping, std::size_t tcp_timeout) {
    // create modbus object
    modbus = modbus_new_tcp(ip.c_str(), static_cast<int>(port));
    if (modbus == nullptr) {
        const std::string error_msg = modbus_strerror(errno);
        throw std::runtime_error("failed to create modbus instance: " + error_msg);
    }

    if (mapping == nullptr) {
        // create new mapping with the maximum number of registers
        this->mapping = modbus_mapping_new(MAX_REGS, MAX_REGS, MAX_REGS, MAX_REGS);
        if (this->mapping == nullptr) {
            const std::string error_msg = modbus_strerror(errno);
            modbus_free(modbus);
            throw std::runtime_error("failed to allocate memory: " + error_msg);
        }
        delete_mapping = true;
    } else {
        // use the provided mapping object
        this->mapping  = mapping;
        delete_mapping = false;
    }

    // create tcp socket
    socket = modbus_tcp_listen(modbus, 1);
    if (socket == -1) {
        const std::string error_msg = modbus_strerror(errno);
        throw std::runtime_error("failed to create tcp socket: " + error_msg);
    }

    // set socket options
    int keepalive = 1;
    int tmp       = setsockopt(socket, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive));
    if (tmp != 0) {
        throw std::system_error(errno, std::generic_category(), "Failed to set socket option SO_KEEPALIVE");
    }

    if (tcp_timeout) {
        unsigned user_timeout = static_cast<unsigned>(tcp_timeout) * 1000;
        tmp                   = setsockopt(socket, IPPROTO_TCP, TCP_USER_TIMEOUT, &user_timeout, sizeof(keepalive));
        if (tmp != 0) {
            throw std::system_error(errno, std::generic_category(), "Failed to set socket option TCP_USER_TIMEOUT");
        }

        unsigned keepidle = 1;
        tmp               = setsockopt(socket, IPPROTO_TCP, TCP_KEEPIDLE, &keepidle, sizeof(keepidle));
        if (tmp != 0) {
            throw std::system_error(errno, std::generic_category(), "Failed to set socket option TCP_KEEPIDLE");
        }

        unsigned keepintvl = 1;
        tmp                = setsockopt(socket, IPPROTO_TCP, TCP_KEEPINTVL, &keepintvl, sizeof(keepintvl));
        if (tmp != 0) {
            throw std::system_error(errno, std::generic_category(), "Failed to set socket option TCP_KEEPINTVL");
        }

        unsigned keepcnt = static_cast<unsigned>(tcp_timeout);
        tmp              = setsockopt(socket, IPPROTO_TCP, TCP_KEEPCNT, &keepcnt, sizeof(keepcnt));
        if (tmp != 0) {
            throw std::system_error(errno, std::generic_category(), "Failed to set socket option TCP_KEEPCNT");
        }
    }
}

Slave::~Slave() {
    if (modbus != nullptr) {
        modbus_close(modbus);
        modbus_free(modbus);
    }
    if (mapping != nullptr && delete_mapping) modbus_mapping_free(mapping);
    if (socket != -1) { close(socket); }
}

void Slave::set_debug(bool debug) {
    if (modbus_set_debug(modbus, debug)) {
        const std::string error_msg = modbus_strerror(errno);
        throw std::runtime_error("failed to enable modbus debugging mode: " + error_msg);
    }
}

void Slave::connect_client() {
    int tmp = modbus_tcp_accept(modbus, &socket);
    if (tmp < 0) {
        const std::string error_msg = modbus_strerror(errno);
        throw std::runtime_error("modbus_tcp_accept failed: " + error_msg);
    }
}

bool Slave::handle_request() {
    // receive modbus request
    uint8_t query[MODBUS_TCP_MAX_ADU_LENGTH];
    int     rc = modbus_receive(modbus, query);

    if (rc > 0) {
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

void Slave::set_byte_timeout(double timeout) {
    const auto T   = double_to_timeout_t(timeout);
    auto       ret = modbus_set_byte_timeout(modbus, T.sec, T.usec);

    if (ret != 0) {
        const std::string error_msg = modbus_strerror(errno);
        throw std::runtime_error("modbus_receive failed: " + error_msg + ' ' + std::to_string(errno));
    }
}

void Slave::set_response_timeout(double timeout) {
    const auto T   = double_to_timeout_t(timeout);
    auto       ret = modbus_set_response_timeout(modbus, T.sec, T.usec);

    if (ret != 0) {
        const std::string error_msg = modbus_strerror(errno);
        throw std::runtime_error("modbus_receive failed: " + error_msg + ' ' + std::to_string(errno));
    }
}

double Slave::get_byte_timeout() {
    timeout_t timeout {};

    auto ret = modbus_get_byte_timeout(modbus, &timeout.sec, &timeout.usec);

    if (ret != 0) {
        const std::string error_msg = modbus_strerror(errno);
        throw std::runtime_error("modbus_receive failed: " + error_msg + ' ' + std::to_string(errno));
    }

    return static_cast<double>(timeout.sec) + (static_cast<double>(timeout.usec) / (1000.0 * 1000.0));
}

double Slave::get_response_timeout() {
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
