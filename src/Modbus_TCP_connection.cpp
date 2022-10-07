/*
 * Copyright (C) 2022 Nikolas Koesling <nikolas@koesling.info>.
 * This program is free software. You can redistribute it and/or modify it under the terms of the MIT License.
 */

#include "Modbus_TCP_connection.hpp"

#include <cstring>
#include <sys/poll.h>

namespace Modbus {
namespace TCP {

bool Connection::handle_request() {
    struct pollfd fd;
    memset(&fd, 0, sizeof(fd));
    fd.fd     = socket;
    fd.events = POLL_IN;
    do {
        int tmp = poll(&fd, 1, -1);
        if (tmp <= 0) {
            if (errno == EINTR) continue;
            throw std::system_error(errno, std::generic_category(), "Failed to poll client socket");
        } else
            break;
    } while (true);


    std::lock_guard<std::mutex> guard(modbus_lock);

    // set client socket
    int restore_socket = modbus_get_socket(modbus);
    modbus_set_socket(modbus, socket);

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
            modbus_set_socket(modbus, restore_socket);
            const std::string error_msg = modbus_strerror(errno);
            throw std::runtime_error("modbus_reply failed: " + error_msg + ' ' + std::to_string(errno));
        }
    } else if (rc == -1) {
        if (errno == ECONNRESET) {
            modbus_set_socket(modbus, restore_socket);
            return true;
        }

        modbus_set_socket(modbus, restore_socket);
        const std::string error_msg = modbus_strerror(errno);
        throw std::runtime_error("modbus_receive failed: " + error_msg + ' ' + std::to_string(errno));
    }

    modbus_set_socket(modbus, restore_socket);
    return false;
}

}  // namespace TCP
}  // namespace Modbus
