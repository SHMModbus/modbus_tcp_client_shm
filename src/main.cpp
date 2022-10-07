/*
 * Copyright (C) 2021-2022 Nikolas Koesling <nikolas@koesling.info>.
 * This program is free software. You can redistribute it and/or modify it under the terms of the MIT License.
 */

#include <atomic>
#include <condition_variable>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <memory>
#include <sysexits.h>
#include <thread>
#include <unistd.h>
#include <unordered_set>
#include <vector>

// cxxopts, but all warnings disabled
#ifdef COMPILER_CLANG
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Weverything"
#elif defined(COMPILER_GCC)
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wall"
#endif

#include <cxxopts.hpp>

#ifdef COMPILER_CLANG
#    pragma clang diagnostic pop
#elif defined(COMPILER_GCC)
#    pragma GCC diagnostic pop
#endif

#include "Modbus_TCP_Client.hpp"
#include "license.hpp"
#include "modbus_shm.hpp"

//! Maximum number of registers per type
constexpr size_t MODBUS_MAX_REGS = 0x10000;

//! terminate flag
static volatile bool terminate = false;

//! modbus socket (to be closed if termination is requested)
static int socket = -1;

/*! \brief signal handler (SIGINT and SIGTERM)
 *
 */
static void sig_term_handler(int) {
    if (socket != -1) close(socket);
    terminate = true;
}

constexpr std::array<int, 10> TERM_SIGNALS = {SIGINT,
                                              SIGTERM,
                                              SIGHUP,
                                              SIGIO,  // should not happen
                                              SIGPIPE,
                                              SIGPOLL,  // should not happen
                                              SIGPROF,  // should not happen
                                              SIGUSR1,
                                              SIGUSR2,
                                              SIGVTALRM};

/*! \brief main function
 *
 * @param argc number of arguments
 * @param argv arguments as char* array
 * @return exit code
 */
int main(int argc, char **argv) {
    const std::string exe_name = std::filesystem::path(argv[0]).filename().string();
    cxxopts::Options  options(exe_name, "Modbus client that uses shared memory objects to store its register values");

    auto exit_usage = [&exe_name]() {
        std::cerr << "Use '" << exe_name << " --help' for more information." << std::endl;
        return EX_USAGE;
    };

    auto euid = geteuid();
    if (!euid) std::cerr << "!!!! WARNING: You should not execute this program with root privileges !!!!" << std::endl;

#ifdef COMPILER_CLANG
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wdisabled-macro-expansion"
#endif
    // establish signal handler
    struct sigaction term_sa;
    term_sa.sa_handler = sig_term_handler;
    term_sa.sa_flags   = SA_RESTART;
    sigemptyset(&term_sa.sa_mask);
    for (const auto SIGNO : TERM_SIGNALS) {
        if (sigaction(SIGNO, &term_sa, nullptr)) {
            perror("Failed to establish signal handler");
            return EX_OSERR;
        }
    }
#ifdef COMPILER_CLANG
#    pragma clang diagnostic pop
#endif

    // all command line arguments
    // clang-format off
    options.add_options()("i,host",
                          "host to listen for incoming connections",
                          cxxopts::value<std::string>()->default_value("any"))
                         ("p,service",
                          "service or port to listen for incoming connections",
                          cxxopts::value<std::string>()->default_value("502"))
                         ("n,name-prefix",
                          "shared memory name prefix",
                          cxxopts::value<std::string>()->default_value("modbus_"))
                         ("do-registers",
                          "number of digital output registers",
                          cxxopts::value<std::size_t>()->default_value("65536"))
                         ("di-registers",
                          "number of digital input registers",
                          cxxopts::value<std::size_t>()->default_value("65536"))
                         ("ao-registers",
                          "number of analog output registers",
                          cxxopts::value<std::size_t>()->default_value("65536"))
                         ("ai-registers",
                          "number of analog input registers",
                          cxxopts::value<std::size_t>()->default_value("65536"))
                         ("m,monitor",
                          "output all incoming and outgoing packets to stdout")
                         ("c,connections",
                          "number of allowed simultaneous Modbus Server connections.",
                          cxxopts::value<std::size_t>()->default_value("1"))
                         ("r,reconnect",
                          "do not terminate if no Modbus Server is connected anymore.")
                         ("byte-timeout",
                          "timeout interval in seconds between two consecutive bytes of the same message. "
                           "In most cases it is sufficient to set the response timeout. "
                           "Fractional values are possible.",
                          cxxopts::value<double>())
                         ("response-timeout",
                          "set the timeout interval in seconds used to wait for a response. "
                          "When a byte timeout is set, if the elapsed time for the first byte of response is longer "
                          "than the given timeout, a timeout is detected. "
                          "When byte timeout is disabled, the full confirmation response must be received before "
                          "expiration of the response timeout. "
                          "Fractional values are possible.",
                          cxxopts::value<double>())
#ifdef OS_LINUX
                         ("t,tcp-timeout",
                          "tcp timeout in seconds. Set to 0 to use the system defaults (not recommended).",
                          cxxopts::value<std::size_t>()->default_value("5"))
#endif
                         ("force",
                          "Force the use of the shared memory even if it already exists. "
                          "Do not use this option per default! "
                          "It should only be used if the shared memory of an improperly terminated instance continues "
                          "to exist as an orphan and is no longer used.")
                         ("s,separate",
                          "Use a separate shared memory for requests with the specified client id. "
                          "The the client id (as hex value) is appended to the shared memory prefix (e.g. modbus_fc_DO)"
                          ". You can specify multiple client ids by separating them with ','. "
                          "Use --separate-all to generate separate shared memories for all possible client ids.",
                          cxxopts::value<std::vector<std::uint8_t>>())
                         ("separate-all",
                          "like --separate, but for all client ids (creates 1028 shared memory files! "
                          "check/set 'ulimit -n' before using this option.)")
                         ("h,help",
                          "print usage")
                         ("version",
                          "print version information")
                         ("license",
                          "show licences");
    // clang-format on

    // parse arguments
    cxxopts::ParseResult args;
    try {
        args = options.parse(argc, argv);
    } catch (cxxopts::OptionParseException &e) {
        std::cerr << "Failed to parse arguments: " << e.what() << '.' << std::endl;
        return exit_usage();
    }

    // print usage
    if (args.count("help")) {
        options.set_width(120);
        std::cout << options.help() << std::endl;
        std::cout << std::endl;
        std::cout << "The modbus registers are mapped to shared memory objects:" << std::endl;
        std::cout << "    type | name                      | mb-server-access | shm name" << std::endl;
        std::cout << "    -----|---------------------------|------------------|----------------" << std::endl;
        std::cout << "    DO   | Discrete Output Coils     | read-write       | <name-prefix>DO" << std::endl;
        std::cout << "    DI   | Discrete Input Coils      | read-only        | <name-prefix>DI" << std::endl;
        std::cout << "    AO   | Discrete Output Registers | read-write       | <name-prefix>AO" << std::endl;
        std::cout << "    AI   | Discrete Input Registers  | read-only        | <name-prefix>AI" << std::endl;
        std::cout << std::endl;
        std::cout << "This application uses the following libraries:" << std::endl;
        std::cout << "  - cxxopts by jarro2783 (https://github.com/jarro2783/cxxopts)" << std::endl;
        std::cout << "  - libmodbus by Stéphane Raimbault (https://github.com/stephane/libmodbus)" << std::endl;
        std::cout << "  - cxxshm (https://github.com/NikolasK-source/cxxshm)" << std::endl;
        return EX_OK;
    }

    // print usage
    if (args.count("version")) {
        std::cout << PROJECT_NAME << ' ' << PROJECT_VERSION << " (compiled with " << COMPILER_INFO << " on "
                  << SYSTEM_INFO << ')'
#ifndef OS_LINUX
                  << "-nonlinux"
#endif
                  << std::endl;
        return EX_OK;
    }

    // print licenses
    if (args.count("license")) {
        print_licenses(std::cout);
        return EX_OK;
    }

    // check arguments
    if (args["do-registers"].as<std::size_t>() > MODBUS_MAX_REGS) {
        std::cerr << "to many do-registers (maximum: " << MODBUS_MAX_REGS << ")." << std::endl;
        return exit_usage();
    }

    if (args["di-registers"].as<std::size_t>() > MODBUS_MAX_REGS) {
        std::cerr << "to many di-registers (maximum: " << MODBUS_MAX_REGS << ")." << std::endl;
        return exit_usage();
    }

    if (args["ao-registers"].as<std::size_t>() > MODBUS_MAX_REGS) {
        std::cerr << "to many ao-registers (maximum: " << MODBUS_MAX_REGS << ")." << std::endl;
        return exit_usage();
    }

    if (args["ai-registers"].as<std::size_t>() > MODBUS_MAX_REGS) {
        std::cerr << "to many ai-registers (maximum: " << MODBUS_MAX_REGS << ")." << std::endl;
        return exit_usage();
    }

    const auto CONNECTIONS = args["connections"].as<std::size_t>();
    if (CONNECTIONS == 0) {
        std::cerr << "The number of connections must not be 0" << std::endl;
        return exit_usage();
    }

    const auto SEPARATE     = args.count("separate");
    const auto SEPARATE_ALL = args.count("separate-all");
    if (SEPARATE && SEPARATE_ALL) {
        std::cerr << "The options --separate and --separate-all cannot be used together." << std::endl;
        return EX_USAGE;
    }

    const auto FORCE_SHM = args.count("force") > 0;

    // create shared memory object for modbus registers
    std::unique_ptr<Modbus::shm::Shm_Mapping> fallback_mapping;
    if (args.count("separate-all") == 0) {
        try {
            fallback_mapping = std::make_unique<Modbus::shm::Shm_Mapping>(args["do-registers"].as<std::size_t>(),
                                                                          args["di-registers"].as<std::size_t>(),
                                                                          args["ao-registers"].as<std::size_t>(),
                                                                          args["ai-registers"].as<std::size_t>(),
                                                                          args["name-prefix"].as<std::string>(),
                                                                          FORCE_SHM);
        } catch (const std::system_error &e) {
            std::cerr << e.what() << std::endl;
            return EX_OSERR;
        }
    }

    std::array<modbus_mapping_t *, Modbus::TCP::MAX_CLIENT_IDS> mb_mappings;
    std::vector<std::unique_ptr<Modbus::shm::Shm_Mapping>>      separate_mappings;

    if (SEPARATE_ALL) {
        for (std::size_t i = 0; i < Modbus::TCP::MAX_CLIENT_IDS; ++i) {
            std::ostringstream sstr;
            sstr << args["name-prefix"].as<std::string>() << std::setfill('0') << std::hex << std::setw(2) << i << '_';

            try {
                separate_mappings.emplace_back(
                        std::make_unique<Modbus::shm::Shm_Mapping>(args["do-registers"].as<std::size_t>(),
                                                                   args["di-registers"].as<std::size_t>(),
                                                                   args["ao-registers"].as<std::size_t>(),
                                                                   args["ai-registers"].as<std::size_t>(),
                                                                   sstr.str(),
                                                                   FORCE_SHM));
                mb_mappings[i] = separate_mappings.back()->get_mapping();
            } catch (const std::system_error &e) {
                std::cerr << e.what() << std::endl;
                return EX_OSERR;
            }
        }
    } else {
        mb_mappings.fill(fallback_mapping->get_mapping());
    }

    if (SEPARATE) {
        auto                        id_list = args["separate"].as<std::vector<uint8_t>>();
        std::unordered_set<uint8_t> id_set(id_list.begin(), id_list.end());

        for (auto a : id_set) {
            std::ostringstream sstr;
            sstr << args["name-prefix"].as<std::string>() << std::setfill('0') << std::hex << std::setw(2)
                 << static_cast<unsigned>(a) << '_';

            try {
                separate_mappings.emplace_back(
                        std::make_unique<Modbus::shm::Shm_Mapping>(args["do-registers"].as<std::size_t>(),
                                                                   args["di-registers"].as<std::size_t>(),
                                                                   args["ao-registers"].as<std::size_t>(),
                                                                   args["ai-registers"].as<std::size_t>(),
                                                                   sstr.str(),
                                                                   FORCE_SHM));
                mb_mappings[a] = separate_mappings.back()->get_mapping();
            } catch (const std::system_error &e) {
                std::cerr << e.what() << std::endl;
                return EX_OSERR;
            }
        }
    }


    // create modbus client
    std::unique_ptr<Modbus::TCP::Client> client;
    try {
        client = std::make_unique<Modbus::TCP::Client>(args["host"].as<std::string>(),
                                                       args["service"].as<std::string>(),
                                                       mb_mappings.data(),
#ifdef OS_LINUX
                                                       args["tcp-timeout"].as<std::size_t>());
#else
                                                       0);
#endif
        client->set_debug(args.count("monitor"));
    } catch (const std::runtime_error &e) {
        std::cerr << e.what() << std::endl;
        return EX_SOFTWARE;
    }
    socket = client->get_socket();

    // set timeouts if required
    try {
        if (args.count("response-timeout")) { client->set_response_timeout(args["response-timeout"].as<double>()); }

        if (args.count("byte-timeout")) { client->set_byte_timeout(args["byte-timeout"].as<double>()); }
    } catch (const std::runtime_error &e) {
        std::cerr << e.what() << std::endl;
        return EX_SOFTWARE;
    }

    auto RECONNECT = args.count("reconnect") != 0;

    std::cerr << "Listening on " << client->get_listen_addr() << " for connections." << std::endl;

    if (CONNECTIONS == 1) {
        // connection loop
        do {
            // connect client
            std::cerr << "Waiting for Modbus Server to establish a connection..." << std::endl;
            std::shared_ptr<Modbus::TCP::Connection> connection;
            try {
                connection = client->connect_client();
            } catch (const std::runtime_error &e) {
                if (!terminate) {
                    std::cerr << e.what() << std::endl;
                    return EX_SOFTWARE;
                }
                break;
            }

            std::cerr << "Modbus Server (" << connection->get_peer() << ") established connection." << std::endl;

            // ========== MAIN LOOP ========== (handle requests)
            bool connection_closed = false;
            while (!terminate && !connection_closed) {
                try {
                    connection_closed = client->handle_request();
                } catch (const std::runtime_error &e) {
                    if (!terminate) std::cerr << e.what() << std::endl;
                    break;
                }
            }

            if (connection_closed)
                std::cerr << "Modbus Server (" << connection->get_peer() << ") closed connection." << std::endl;
        } while (RECONNECT);
    } else {
        std::cerr << "WARNING: Using more than one connection is an experimental feature!" << std::endl;

        std::mutex log_lock;          // mutex for logging
        std::mutex con_finish_mutex;  // mutex for condition_variable 'con_thread_finished'
        std::condition_variable
                con_thread_finished;  // condition variable that is notified when a connection thread terminates
        std::atomic<std::size_t> active_connections;  // number of active connections
        const auto               PID = getpid();      // pid of main thread

        /*
         * Thread that handles a single mosbus tcp connection.
         * It notifies the condition_variable 'con_thread_finished' when the connection was closed
         */
        auto connection_thread = [&log_lock, &active_connections, &con_thread_finished, &con_finish_mutex](
                                         std::shared_ptr<Modbus::TCP::Connection> connection) {
            bool connection_closed = false;
            while (!terminate && !connection_closed) {
                try {
                    connection_closed = connection->handle_request();
                } catch (const std::runtime_error &e) {
                    if (!terminate) std::cerr << e.what() << std::endl;
                    break;
                }
            }

            if (connection_closed) {
                std::lock_guard<decltype(log_lock)> log_guard(log_lock);
                std::cerr << "Modbus Server (" << connection->get_peer() << ") closed connection." << std::endl;
            }

            {
                std::lock_guard<decltype(con_finish_mutex)> guard(con_finish_mutex);
                --active_connections;
            }
            con_thread_finished.notify_all();
        };

        /*
         * Watchdog thread that monitors the number of active connections.
         * It signals SIGINT to the main thread once all connections are closed.
         * This thread is only started if the --reconnect option is not set.
         */
        auto watchdog_thread =
                [&con_finish_mutex, CONNECTIONS, &active_connections, &log_lock, &con_thread_finished, PID] {
                    std::unique_lock lock(con_finish_mutex);
                    con_thread_finished.wait(lock,
                                             [&active_connections, CONNECTIONS] { return active_connections == 0; });

                    {
                        std::lock_guard<decltype(log_lock)> log_guard(log_lock);
                        std::cerr << "Last active connection closed." << std::endl;
                    }

                    if (kill(PID, SIGINT)) {
                        perror("kill");
                        exit(EX_OSERR);
                    };
                };

        std::unique_ptr<std::thread> connection_watchdog;  // connection watchdog thread

        do {
            // accept connection
            {
                std::lock_guard<decltype(log_lock)> log_guard(log_lock);
                std::cerr << "Waiting for Modbus Server to establish a connection..." << std::endl;
            }
            std::shared_ptr<Modbus::TCP::Connection> connection;
            try {
                connection = client->connect_client();
                ++active_connections;
            } catch (const std::runtime_error &e) {
                if (!terminate) {
                    std::cerr << e.what() << std::endl;
                    return EX_SOFTWARE;
                }
                break;
            }

            {
                std::lock_guard<decltype(log_lock)> log_guard(log_lock);
                std::cerr << "Modbus Server (" << connection->get_peer() << ") established connection." << std::endl;
            }

            // start watchdog if --reconnect is not set and the watchdog is not already started
            if (!RECONNECT && !connection_watchdog) {
                connection_watchdog = std::make_unique<std::thread>(watchdog_thread);
            }

            // start connection thread
            std::thread thread(connection_thread, connection);
            thread.detach();

            // check if more connections are possible. If not wait for condition_variable 'con_thread_finished'
            std::unique_lock lock(con_finish_mutex);
            if (active_connections >= CONNECTIONS) {
                {
                    std::lock_guard<decltype(log_lock)> log_guard(log_lock);
                    std::cerr << "Waiting for available connection slot..." << std::endl;
                }
                con_thread_finished.wait(
                        lock, [&active_connections, CONNECTIONS] { return active_connections < CONNECTIONS; });
            }
            lock.unlock();
        } while (true);

        // wait for connection threads
        std::unique_lock lock(con_finish_mutex);
        if (active_connections != 0) {
            {
                std::lock_guard<decltype(log_lock)> log_guard(log_lock);
                std::cerr << "Waiting for connection threads to terminate..." << std::endl;
            }
            con_thread_finished.wait(lock,
                                     [&active_connections, CONNECTIONS] { return active_connections < CONNECTIONS; });
        }
        lock.unlock();

        // wait for watchdog thread
        if (connection_watchdog) { connection_watchdog->join(); }
    }

    std::cerr << "Terminating..." << std::endl;
}
