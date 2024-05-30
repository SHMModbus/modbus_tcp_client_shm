/*
 * Copyright (C) 2022 Nikolas Koesling <nikolas@koesling.info>.
 * This program is free software. You can redistribute it and/or modify it under the terms of the GPLv3 License.
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cxxsemaphore.hpp>
#include <memory>
#include <modbus/modbus.h>
#include <string>
#include <sys/poll.h>
#include <unistd.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>


namespace Modbus::TCP {

class Client_Poll {
public:
    static constexpr std::size_t MAX_CLIENT_IDS = 256;

    enum class run_t : std::uint8_t { ok, term_signal, term_nocon, timeout, interrupted, semaphore };

private:
    const std::size_t          max_clients;
    std::vector<struct pollfd> poll_fds;

    bool debug = false;  //!< modbus debugging enabled

    modbus_t *modbus;  //!< modbus object (see libmodbus library)
    std::array<modbus_mapping_t *, MAX_CLIENT_IDS>
                      mappings {};         //!< modbus data objects (one per possible client id) (see libmodbus library)
    modbus_mapping_t *delete_mapping;      //!< contains a pointer to a mapping that is to be deleted
    int               server_socket = -1;  //!< socket of the modbus connection
    std::unordered_map<int, std::string> client_addrs;

    std::unique_ptr<cxxsemaphore::Semaphore> semaphore;

    long semaphore_error_counter = 0;

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
    Client_Poll(const std::string                              &host,
                const std::string                              &service,
                std::array<modbus_mapping_t *, MAX_CLIENT_IDS> &mappings,
                std::size_t                                     tcp_timeout = 5,
                std::size_t                                     max_clients = 1);

    /**
     * @brief destroy the modbus client
     */
    ~Client_Poll();

    Client_Poll(const Client_Poll &other)           = delete;
    Client_Poll(Client_Poll &&other)                = delete;
    Client_Poll operator&(const Client_Poll &other) = delete;
    Client_Poll operator&(Client_Poll &&other)      = delete;

    /**
     * @brief use the semaphore mechanism
     *
     * @param name name of the shared
     * @param force use the semaphore even if it already exists
     */
    void enable_semaphore(const std::string &name, bool force = false);

    /*! \brief enable/disable debugging output
     *
     * @param enable_debug true: enable debug output
     */
    void set_debug(bool enable_debug);

    /** \brief get the address the tcp server is listening on
     *
     * @return server listening address
     */
    std::string get_listen_addr() const;

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
    [[maybe_unused]] double get_byte_timeout();

    /**
     * \brief get response timeout in seconds
     * @return response timeout
     */
    [[maybe_unused]] double get_response_timeout();

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
     * @param mb_function_callback callback function that is called with the modbus function code on each modbus
     * telegram
     * @return true continue
     * @return false terminate
     */
    run_t run(int  signal_fd,
              bool reconnect                                         = true,
              int  timeout                                           = -1,
              void (*mb_function_callback)(uint8_t mb_function_code) = nullptr);

private:
#ifdef OS_LINUX
    void set_tcp_timeout(std::size_t tcp_timeout) const;
#endif

    void listen();
};

}  // namespace Modbus::TCP
