#include "Modbus_TCP_Slave.hpp"

#include <stdexcept>
#include <unistd.h>

namespace Modbus {
namespace TCP {

static constexpr int MAX_REGS = 0x10000;

Slave::Slave(const std::string &ip, unsigned short port, modbus_mapping_t *mapping) {
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
        modbus_reply(modbus, query, rc, mapping);
    } else if (rc == -1) {
        if (errno == ECONNRESET) return true;

        const std::string error_msg = modbus_strerror(errno);
        throw std::runtime_error("modbus_receive failed: " + error_msg + ' ' + std::to_string(errno));
    }

    return false;
}

}  // namespace TCP
}  // namespace Modbus
