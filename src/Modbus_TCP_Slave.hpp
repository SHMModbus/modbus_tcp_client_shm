/*
 * Copyright (C) 2021-2022 Nikolas Koesling <nikolas@koesling.info>.
 * This program is free software. You can redistribute it and/or modify it under the terms of the MIT License.
 */

#pragma once

#include <modbus/modbus.h>
#include <string>

namespace Modbus {
namespace TCP {

//! Modbus TCP slave
class Slave {
private:
    modbus_t         *modbus;          //!< modbus object (see libmodbus library)
    modbus_mapping_t *mapping;         //!< modbus data object (see libmodbus library)
    bool              delete_mapping;  //!< indicates whether the mapping object was created by this instance
    int               socket = -1;     //!< socket of the modbus connection

public:
    /*! \brief create modbus slave (TCP server)
     *
     * @param ip ip to listen for incoming connections (default 0.0.0.0 (any))
     * @param port port to listen  for incoming connections (default 502)
     * @param mapping modbus mapping object (nullptr: an mapping object with maximum size is generated)
     */
    explicit Slave(const std::string &ip          = "0.0.0.0",
                   short unsigned int port        = 502,
                   modbus_mapping_t  *mapping     = nullptr,
                   std::size_t        tcp_timeout = 5);

    /*! \brief destroy the modbus slave
     *
     */
    ~Slave();

    /*! \brief enable/disable debugging output
     *
     * @param debug true: enable debug output
     */
    void set_debug(bool debug);

    /*! \brief wait for client to connect
     *
     * @return ip of the connected client
     */
    std::string connect_client();

    /*! \brief wait for request from Master and generate reply
     *
     * @return true: connection closed
     */
    bool handle_request();

    /*!
     * \brief set byte timeout
     *
     * @details see https://libmodbus.org/docs/v3.1.7/modbus_set_byte_timeout.html
     *
     * @param timeout byte timeout in seconds
     */
    void set_byte_timeout(double timeout);

    /*!
     * \brief set byte timeout
     *
     * @details see https://libmodbus.org/docs/v3.1.7/modbus_set_response_timeout.html
     *
     * @param timeout byte response in seconds
     */
    void set_response_timeout(double timeout);

    /**
     * \brief get byte timeout in seconds
     * @return byte timeout
     */
    double get_byte_timeout();

    /**
     * \brief get response timeout in seconds
     * @return response timeout
     */
    double get_response_timeout();

    /*! \brief get the modbus socket
     *
     * @return socket of the modbus connection
     */
    [[nodiscard]] int get_socket() const noexcept { return socket; }
};

}  // namespace TCP
}  // namespace Modbus
