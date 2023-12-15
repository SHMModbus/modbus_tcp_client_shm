/*
 * Copyright (C) 2022 Nikolas Koesling <nikolas@koesling.info>.
 * This program is free software. You can redistribute it and/or modify it under the terms of the MIT License.
 */
#pragma once

#include <cstddef>
#include <cxxsemaphore.hpp>
#include <memory>
#include <modbus/modbus.h>
#include <string>
#include <sys/poll.h>
#include <unistd.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Modbus {
namespace TCP {

class Client_Poll {
public:
    static constexpr std::size_t MAX_CLIENT_IDS = 256;

    enum class run_t {
        term_signal,
        term_nocon,
        ok,
        timeout,
        interrupted,
    };

private:
    const std::size_t          max_clients;
    std::vector<struct pollfd> poll_fds;

    modbus_t *modbus;  //!< modbus object (see libmodbus library)
    modbus_mapping_t
            *mappings[MAX_CLIENT_IDS];  //!< modbus data objects (one per possible client id) (see libmodbus library)
    modbus_mapping_t                    *delete_mapping;      //!< contains a pointer to a mapping that is to be deleted
    int                                  server_socket = -1;  //!< socket of the modbus connection
    std::unordered_map<int, std::string> client_addrs;

    std::unique_ptr<cxxsemaphore::Semaphore> semaphore;

public:
    /*! \brief create modbus client (TCP server)
     *
     * @param host host to listen for incoming connections (default 0.0.0.0 (any))
     * @param service service/port to listen  for incoming connections (default 502)
     * @param mapping modbus mapping object for all client ids
     *                nullptr: an mapping object with maximum size is generated
     * @param tcp_timeout tcp timeout (currently only available on linux systems)
     */
    explicit Client_Poll(const std::string &host        = "any",
                         const std::string &service     = "502",
                         modbus_mapping_t  *mapping     = nullptr,
                         std::size_t        tcp_timeout = 5,
                         std::size_t        max_clients = 1);

    /**
     * @brief create modbus client (TCP server) with dedicated mappings per client id
     *
     * @param host host to listen for incoming connections
     * @param service service/port to listen  for incoming connections
     * @param mappings modbus mappings (one for each possible id)
     * @param tcp_timeout tcp timeout (currently only available on linux systems)
     */
    Client_Poll(const std::string &host,
                const std::string &service,
                modbus_mapping_t  *mappings[MAX_CLIENT_IDS],
                std::size_t        tcp_timeout = 5,
                std::size_t        max_clients = 1);

    /**
     * @brief destroy the modbus client
     */
    ~Client_Poll();

    /**
     * @brief use the semaphore mechanism
     *
     * @param name name of the shared
     * @param force use the semaphore even if it already exists
     */
    void enable_semaphore(const std::string &name, bool force = false);

    /*! \brief enable/disable debugging output
     *
     * @param debug true: enable debug output
     */
    void set_debug(bool debug);

    /** \brief get the address the tcp server is listening on
     *
     * @return server listening address
     */
    std::string get_listen_addr();

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
    [[nodiscard]] int get_socket() const noexcept { return server_socket; }

    /**
     * @brief perform one update cycle
     * @param reconnect false: terminate once the last active connection disconnects
     *                  true: continue listening for new connections if there is no client (Mosbus Server) left
     * @param timeout timeout valoue for call of poll (see: man 2 poll)
     * @param signal_fd signal file descriptor for termination signals
     * @return true continue
     * @return false terminate
     */
    run_t run(int signal_fd, bool reconnect = true, int timeout = -1);

private:
#ifdef OS_LINUX
    void set_tcp_timeout(std::size_t tcp_timeout);
#endif

    void listen();
};

}  // namespace TCP
}  // namespace Modbus
