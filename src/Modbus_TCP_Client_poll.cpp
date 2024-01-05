/*
 * Copyright (C) 2022 Nikolas Koesling <nikolas@koesling.info>.
 * This program is free software. You can redistribute it and/or modify it under the terms of the MIT License.
 */

#include "Modbus_TCP_Client_poll.hpp"

#include "Print_Time.hpp"
#include "sa_to_str.hpp"

#include <iostream>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sstream>
#include <stdexcept>
#include <sys/socket.h>
#include <system_error>

namespace Modbus {
namespace TCP {

//* maximum number of Modbus registers (per type)
static constexpr int MAX_REGS = 0x10000;

//* value to increment error counter if semaphore could not be acquired
static constexpr long SEMAPHORE_ERROR_INC = 10;

//* value to decrement error counter if semaphore could be acquired
static constexpr long SEMAPHORE_ERROR_DEC = 1;

//* maximum value of semaphore error counter
static constexpr long SEMAPHORE_ERROR_MAX = 1000;

//* maximum time to wait for semaphore (100ms)
static constexpr struct timespec SEMAPHORE_MAX_TIME = {0, 100'000};

Client_Poll::Client_Poll(const std::string &host,
                         const std::string &service,
                         modbus_mapping_t  *mapping,
                         std::size_t        tcp_timeout,
                         std::size_t        max_clients)
    : max_clients(max_clients), poll_fds(max_clients + 2, {0, 0, 0}) {
    const char *host_str = "::";
    if (!(host.empty() || host == "any")) host_str = host.c_str();

    // create modbus object
    modbus = modbus_new_tcp_pi(host_str, service.c_str());
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
        this->mappings[i] = mb_mapping;
    }

    listen();

#ifdef OS_LINUX
    if (tcp_timeout) set_tcp_timeout(tcp_timeout);
#else
    static_cast<void>(tcp_timeout);
#endif
}

Client_Poll::Client_Poll(const std::string &host,
                         const std::string &service,
                         modbus_mapping_t **mappings,
                         std::size_t        tcp_timeout,
                         std::size_t        max_clients)
    : max_clients(max_clients), poll_fds(max_clients + 2, {0, 0, 0}) {
    const char *host_str = "::";
    if (!(host.empty() || host == "any")) host_str = host.c_str();

    // create modbus object
    modbus = modbus_new_tcp_pi(host_str, service.c_str());
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

void Client_Poll::listen() {
    // create tcp socket
    server_socket = modbus_tcp_pi_listen(modbus, 1);
    if (server_socket == -1) {
        if (errno == ECONNREFUSED) {
            throw std::runtime_error("failed to create tcp socket: unknown or invalid service");
        } else {
            const std::string error_msg = modbus_strerror(errno);
            throw std::runtime_error("failed to create tcp socket: " + error_msg);
        }
    }

    // set socket options
    // enable socket keepalive (--> fail if connection partner is not reachable)
    int keepalive = 1;
    int tmp       = setsockopt(server_socket, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive));
    if (tmp != 0) {
        throw std::system_error(errno, std::generic_category(), "Failed to set socket option SO_KEEPALIVE");
    }
}

Client_Poll::~Client_Poll() {
    if (modbus != nullptr) {
        modbus_close(modbus);
        modbus_free(modbus);
    }
    if (delete_mapping) modbus_mapping_free(delete_mapping);
    if (server_socket != -1) { close(server_socket); }
}

#ifdef OS_LINUX
void Client_Poll::set_tcp_timeout(std::size_t tcp_timeout) {
    // set user timeout (~= timeout for tcp connection)
    unsigned user_timeout = static_cast<unsigned>(tcp_timeout) * 1000;
    int      tmp = setsockopt(server_socket, IPPROTO_TCP, TCP_USER_TIMEOUT, &user_timeout, sizeof(tcp_timeout));
    if (tmp != 0) {
        throw std::system_error(errno, std::generic_category(), "Failed to set socket option TCP_USER_TIMEOUT");
    }

    // start sending keepalive request after one second without request
    unsigned keepidle = 1;
    tmp               = setsockopt(server_socket, IPPROTO_TCP, TCP_KEEPIDLE, &keepidle, sizeof(keepidle));
    if (tmp != 0) {
        throw std::system_error(errno, std::generic_category(), "Failed to set socket option TCP_KEEPIDLE");
    }

    // send up to 5 keepalive requests during the timeout time, but not more than one per second
    unsigned keepintvl = std::max(static_cast<unsigned>(tcp_timeout / 5), 1u);
    tmp                = setsockopt(server_socket, IPPROTO_TCP, TCP_KEEPINTVL, &keepintvl, sizeof(keepintvl));
    if (tmp != 0) {
        throw std::system_error(errno, std::generic_category(), "Failed to set socket option TCP_KEEPINTVL");
    }

    // 5 keepalive requests if the timeout time is >= 5s; else send one request each second
    unsigned keepcnt = std::min(static_cast<unsigned>(tcp_timeout), 5u);
    tmp              = setsockopt(server_socket, IPPROTO_TCP, TCP_KEEPCNT, &keepcnt, sizeof(keepcnt));
    if (tmp != 0) {
        throw std::system_error(errno, std::generic_category(), "Failed to set socket option TCP_KEEPCNT");
    }
}
#endif

void Client_Poll::enable_semaphore(const std::string &name, bool force) {
    if (semaphore) throw std::logic_error("semaphore already enabled");

    semaphore = std::make_unique<cxxsemaphore::Semaphore>(name, 1, force);
}

void Client_Poll::set_debug(bool debug) {
    if (modbus_set_debug(modbus, debug)) {
        const std::string error_msg = modbus_strerror(errno);
        throw std::runtime_error("failed to enable modbus debugging mode: " + error_msg);
    }
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

void Client_Poll::set_byte_timeout(double timeout) {
    const auto T   = double_to_timeout_t(timeout);
    auto       ret = modbus_set_byte_timeout(modbus, T.sec, T.usec);

    if (ret != 0) {
        const std::string error_msg = modbus_strerror(errno);
        throw std::runtime_error("modbus_receive failed: " + error_msg + ' ' + std::to_string(errno));
    }
}

void Client_Poll::set_response_timeout(double timeout) {
    const auto T   = double_to_timeout_t(timeout);
    auto       ret = modbus_set_response_timeout(modbus, T.sec, T.usec);

    if (ret != 0) {
        const std::string error_msg = modbus_strerror(errno);
        throw std::runtime_error("modbus_receive failed: " + error_msg + ' ' + std::to_string(errno));
    }
}

double Client_Poll::get_byte_timeout() {
    timeout_t timeout {};

    auto ret = modbus_get_byte_timeout(modbus, &timeout.sec, &timeout.usec);

    if (ret != 0) {
        const std::string error_msg = modbus_strerror(errno);
        throw std::runtime_error("modbus_receive failed: " + error_msg + ' ' + std::to_string(errno));
    }

    return static_cast<double>(timeout.sec) + (static_cast<double>(timeout.usec) / (1000.0 * 1000.0));
}

double Client_Poll::get_response_timeout() {
    timeout_t timeout {};

    auto ret = modbus_get_response_timeout(modbus, &timeout.sec, &timeout.usec);

    if (ret != 0) {
        const std::string error_msg = modbus_strerror(errno);
        throw std::runtime_error("modbus_receive failed: " + error_msg + ' ' + std::to_string(errno));
    }

    return static_cast<double>(timeout.sec) + (static_cast<double>(timeout.usec) / (1000.0 * 1000.0));
}

Client_Poll::run_t Client_Poll::run(int signal_fd, bool reconnect, int timeout) {
    std::size_t i = 0;

    // poll signal fd
    {
        auto &fd  = poll_fds[i++];
        fd.fd     = signal_fd;
        fd.events = POLLIN;
    }

    // do not poll server socket if maximum number of connections is reached
    const auto active_clients = client_addrs.size();
    const bool poll_server    = active_clients < max_clients;
    if (poll_server) {
        auto &fd  = poll_fds[i++];
        fd.fd     = server_socket;
        fd.events = POLLIN;
    }

    // add client sockets to poll
    for (auto con : client_addrs) {
        auto &fd  = poll_fds[i++];
        fd.fd     = con.first;
        fd.events = POLLIN;
    }

    // number of files to poll
    const nfds_t poll_size = active_clients + (poll_server ? 2 : 1);

    int tmp = poll(poll_fds.data(), poll_size, timeout);
    if (tmp == -1) {
        if (errno == EINTR) return run_t::interrupted;
        throw std::system_error(errno, std::generic_category(), "Failed to poll socket(s)");
    } else if (tmp == 0) {
        // poll timed out
        return run_t::timeout;
    }

    i = 0;
    {
        auto &fd = poll_fds[i++];
        if (fd.revents) {
            if (fd.revents & POLLNVAL) throw std::logic_error("poll (server socket) returned POLLNVAL");
            if (fd.revents & POLLERR) throw std::logic_error("poll (signal fd) returned POLLERR");
            if (fd.revents & POLLHUP) throw std::logic_error("poll (signal fd) returned POLLHUP");
            if (fd.revents & POLLIN) return run_t::term_signal;
            std::ostringstream sstr;
            sstr << "poll (signal fd) returned unknown revent: " << fd.revents;
            throw std::logic_error(sstr.str());
        }
    }

    if (poll_server) {
        auto &fd = poll_fds[i++];

        if (fd.revents) {
            if (fd.revents & POLLNVAL) throw std::logic_error("poll (server socket) returned POLLNVAL");
            else if (fd.revents & POLLHUP)
                throw std::logic_error("poll (server socket) returned POLLHUP");
            else if (fd.revents & POLLIN || fd.revents & POLLERR) {
                tmp = modbus_tcp_pi_accept(modbus, &server_socket);
                if (tmp < 0) {
                    const std::string error_msg = modbus_strerror(errno);
                    throw std::runtime_error("modbus_tcp_accept failed: " + error_msg);
                }

                auto client_socket = modbus_get_socket(modbus);

                struct sockaddr_storage peer_addr;
                socklen_t               len = sizeof(peer_addr);
                tmp = getpeername(client_socket, reinterpret_cast<struct sockaddr *>(&peer_addr), &len);

                if (tmp < 0) {
                    const std::string error_msg = modbus_strerror(errno);
                    throw std::runtime_error("getpeername failed: " + error_msg);
                }

                std::ostringstream sstr;

                sstr << sockaddr_to_str(peer_addr);
                // the port entries have the same offset and size in sockaddr_in and sockaddr_in6
                sstr << ':' << htons(reinterpret_cast<const struct sockaddr_in *>(&peer_addr)->sin_port);

                client_addrs[client_socket] = sstr.str();
                std::cerr << Print_Time::iso << " INFO: [" << active_clients + 1 << "] Modbus Server (" << sstr.str()
                          << ") established connection." << std::endl;
            } else {
                std::ostringstream sstr;
                sstr << "poll (server socket) returned unknown revent: " << fd.revents;
                throw std::logic_error(sstr.str());
            }
        }
    }

    for (; i < poll_size; ++i) {
        auto &fd = poll_fds[i];

        auto close_con = [&fd](auto &client_addrs) {
            close(fd.fd);
            std::cerr << Print_Time::iso << " INFO: [" << client_addrs.size() - 1 << "] Modbus server ("
                      << client_addrs[fd.fd] << ") connection closed." << std::endl;
            client_addrs.erase(fd.fd);
        };

        if (fd.revents) {
            if (fd.revents & POLLNVAL) {
                std::ostringstream sstr;
                sstr << "poll (client socket: " << client_addrs.at(fd.fd) << ") returned POLLNVAL";
                throw std::logic_error(sstr.str());
            }

            if (fd.revents & POLLHUP & !(fd.revents & POLLERR)) {
                close_con(client_addrs);
            } else if (fd.revents & POLLIN || fd.revents & POLLERR) {
                modbus_set_socket(modbus, fd.fd);

                uint8_t query[MODBUS_TCP_MAX_ADU_LENGTH];
                int     rc = modbus_receive(modbus, query);

                if (rc > 0) {
                    const auto CLIENT_ID = query[6];

                    // get mapping
                    auto mapping = mappings[CLIENT_ID];

                    // handle request
                    if (semaphore) {
                        if (!semaphore->wait(SEMAPHORE_MAX_TIME)) {
                            std::cerr << Print_Time::iso << " WARNING: Failed to acquire semaphore '"
                                      << semaphore->get_name() << "' within 100ms." << std::endl;

                            semaphore_error_counter += SEMAPHORE_ERROR_INC;

                            if (semaphore_error_counter >= SEMAPHORE_ERROR_MAX) {
                                std::cerr << Print_Time::iso << "ERROR: Repeatedly failed to acquire the semaphore"
                                          << std::endl;
                                close_con(client_addrs);
                                return run_t::semaphore;
                            }
                        } else {
                            semaphore_error_counter -= SEMAPHORE_ERROR_DEC;
                            if (semaphore_error_counter < 0) semaphore_error_counter = 0;
                        }
                    }

                    int ret = modbus_reply(modbus, query, rc, mapping);
                    if (semaphore && semaphore->is_acquired()) semaphore->post();

                    if (ret == -1) {
                        std::cerr << Print_Time::iso << " ERROR: modbus_reply failed: " << modbus_strerror(errno)
                                  << std::endl;
                        close_con(client_addrs);
                    }
                } else if (rc == -1) {
                    if (errno != ECONNRESET) {
                        std::cerr << Print_Time::iso << " ERROR: modbus_receive failed: " << modbus_strerror(errno)
                                  << std::endl;
                    }
                    close_con(client_addrs);
                } else {  // rc == 0
                    close_con(client_addrs);
                }
            }
        }
    }

    // check if there are any connections
    if (!reconnect) {
        if (client_addrs.empty()) return run_t::term_nocon;
    }

    return run_t::ok;
}

std::string Client_Poll::get_listen_addr() {
    struct sockaddr_storage sock_addr;
    socklen_t               len = sizeof(sock_addr);
    int                     tmp = getsockname(server_socket, reinterpret_cast<struct sockaddr *>(&sock_addr), &len);

    if (tmp < 0) {
        const std::string error_msg = modbus_strerror(errno);
        throw std::runtime_error("getsockname failed: " + error_msg);
    }

    std::ostringstream sstr;
    sstr << sockaddr_to_str(sock_addr);
    // the port entries have the same offset and size in sockaddr_in and sockaddr_in6
    sstr << ':' << htons(reinterpret_cast<const struct sockaddr_in *>(&sock_addr)->sin_port);

    return sstr.str();
}

}  // namespace TCP
}  // namespace Modbus