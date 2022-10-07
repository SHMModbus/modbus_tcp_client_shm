/*
 * Copyright (C) 2022 Nikolas Koesling <nikolas@koesling.info>.
 * This program is free software. You can redistribute it and/or modify it under the terms of the MIT License.
 */

#pragma once

#include <modbus/modbus.h>
#include <mutex>
#include <string>
#include <utility>

namespace Modbus {
namespace TCP {

class Connection {
private:
    std::string        peer;
    int                socket;
    std::mutex        &modbus_lock;
    modbus_t          *modbus;
    modbus_mapping_t **mappings;

public:
    Connection(std::string peer, int socket, std::mutex &modbus_lock, modbus_t *modbus, modbus_mapping_t **mappings)
        : peer(std::move(peer)), socket(socket), modbus_lock(modbus_lock), modbus(modbus), mappings(mappings) {}

    /**
     * @brief wait for request from Modbus Server and generate reply
     * @return true: connection closed
     */
    bool handle_request();

    /**
     * @brief get connection peer
     * @return connection peer
     */
    [[nodiscard]] const std::string &get_peer() const { return peer; }
};

}  // namespace TCP
}  // namespace Modbus
