/*
 * Copyright (C) 2021-2022 Nikolas Koesling <nikolas@koesling.info>.
 * This program is free software. You can redistribute it and/or modify it under the terms of the GPLv3 License.
 */

#include "Modbus_TCP_Client_poll.hpp"
#include "Print_Time.hpp"
#include "generated/version_info.hpp"
#include "license.hpp"
#include "modbus_shm.hpp"

#include <atomic>
#include <condition_variable>
#include <csignal>
#include <cxxsemaphore_version_info.hpp>
#include <cxxshm_version_info.hpp>
#include <filesystem>
#include <iostream>
#include <modbus/modbus-version.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/signalfd.h>
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

//! Maximum number of registers per type
static constexpr size_t MODBUS_MAX_REGS = 0x10000;

//! Default permissions for the created shared memory
static constexpr mode_t DEFAULT_SHM_PERMISSIONS = 0660;

//! terminate flag
static volatile bool terminate = false;  // NOLINT

/*! \brief signal handler (SIGINT and SIGTERM)
 *
 */
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
    const std::string exe_name = std::filesystem::path(argv[0]).filename().string();  // NOLINT
    cxxopts::Options  options(exe_name, "Modbus client that uses shared memory objects to store its register values");

    auto exit_usage = [&exe_name]() {
        std::cerr << "Use '" << exe_name << " --help' for more information.\n";
        return EX_USAGE;
    };

    auto euid = geteuid();
    if (!euid)
        std::cerr << Print_Time::iso
                  << " WARNING: !!!! You should not execute this program with root privileges !!!!'n";

#ifdef COMPILER_CLANG
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wdisabled-macro-expansion"
#endif
    // create signal fd
    sigset_t sigmask;
    sigemptyset(&sigmask);
    for (const auto SIGNO : TERM_SIGNALS) {
        sigaddset(&sigmask, SIGNO);
    }

    if (sigprocmask(SIG_BLOCK, &sigmask, nullptr) == -1) {
        perror("sigprocmask");
        return EX_OSERR;
    }

    int signal_fd = signalfd(-1, &sigmask, 0);
    if (signal_fd == -1) {
        perror("signal_fd");
        return EX_OSERR;
    }
#ifdef COMPILER_CLANG
#    pragma clang diagnostic pop
#endif

    // all command line arguments
    options.add_options("network")(
            "i,host", "host to listen for incoming connections", cxxopts::value<std::string>()->default_value("any"));
    options.add_options("network")("p,service",
                                   "service or port to listen for incoming connections",
                                   cxxopts::value<std::string>()->default_value("502"));
    options.add_options("shared memory")(
            "n,name-prefix", "shared memory name prefix", cxxopts::value<std::string>()->default_value("modbus_"));
    options.add_options("modbus")("do-registers",
                                  "number of digital output registers",
                                  cxxopts::value<std::size_t>()->default_value("65536"));
    options.add_options("modbus")(
            "di-registers", "number of digital input registers", cxxopts::value<std::size_t>()->default_value("65536"));
    options.add_options("modbus")(
            "ao-registers", "number of analog output registers", cxxopts::value<std::size_t>()->default_value("65536"));
    options.add_options("modbus")(
            "ai-registers", "number of analog input registers", cxxopts::value<std::size_t>()->default_value("65536"));
    options.add_options("modbus")("m,monitor", "output all incoming and outgoing packets to stdout");
    options.add_options("network")("c,connections",
                                   "number of allowed simultaneous Modbus Server connections.",
                                   cxxopts::value<std::size_t>()->default_value("1"));
    options.add_options("network")("r,reconnect", "do not terminate if no Modbus Server is connected anymore.");
    options.add_options("modbus")("byte-timeout",
                                  "timeout interval in seconds between two consecutive bytes of the same message. "
                                  "In most cases it is sufficient to set the response timeout. "
                                  "Fractional values are possible.",
                                  cxxopts::value<double>());
    options.add_options("modbus")(
            "response-timeout",
            "set the timeout interval in seconds used to wait for a response. "
            "When a byte timeout is set, if the elapsed time for the first byte of response is longer "
            "than the given timeout, a timeout is detected. "
            "When byte timeout is disabled, the full confirmation response must be received before "
            "expiration of the response timeout. "
            "Fractional values are possible.",
            cxxopts::value<double>());
#ifdef OS_LINUX
    options.add_options("network")("t,tcp-timeout",
                                   "tcp timeout in seconds. Set to 0 to use the system defaults (not recommended).",
                                   cxxopts::value<std::size_t>()->default_value("5"));
#endif
    options.add_options("shared memory")(
            "force",
            "Force the use of the shared memory even if it already exists. "
            "Do not use this option per default! "
            "It should only be used if the shared memory of an improperly terminated instance continues "
            "to exist as an orphan and is no longer used.");
    options.add_options("shared memory")(
            "s,separate",
            "Use a separate shared memory for requests with the specified client id. "
            "The client id (as hex value) is appended to the shared memory prefix (e.g. modbus_fc_DO)"
            ". You can specify multiple client ids by separating them with ','. "
            "Use --separate-all to generate separate shared memories for all possible client ids.",
            cxxopts::value<std::vector<std::uint8_t>>());
    options.add_options("shared memory")("separate-all",
                                         "like --separate, but for all client ids (creates 1028 shared memory files! "
                                         "check/set 'ulimit -n' before using this option.)");
    options.add_options("shared memory")("semaphore",
                                         "protect the shared memory with a named semaphore against simultaneous access",
                                         cxxopts::value<std::string>());
    options.add_options("shared memory")(
            "semaphore-force",
            "Force the use of the semaphore even if it already exists. "
            "Do not use this option per default! "
            "It should only be used if the semaphore of an improperly terminated instance continues "
            "to exist as an orphan and is no longer used.");
    options.add_options("shared memory")("b,permissions",
                                         "permission bits that are applied when creating a shared memory.",
                                         cxxopts::value<std::string>()->default_value("0640"));
    options.add_options("other")("h,help", "print usage");
    options.add_options("version information")("version", "print version and exit");
    options.add_options("version information")("longversion",
                                               "print version (including compiler and system info) and exit");
    options.add_options("version information")("shortversion", "print version (only version string) and exit");
    options.add_options("version information")("git-hash", "print git hash");
    options.add_options("other")("license", "show licences (short)");
    options.add_options("other")("license-full", "show licences (full license text)");

    // parse arguments
    cxxopts::ParseResult args;
    try {
        args = options.parse(argc, argv);
    } catch (cxxopts::exceptions::parsing::exception &e) {
        std::cerr << Print_Time::iso << " ERROR: Failed to parse arguments: " << e.what() << ".'\n";
        return exit_usage();
    }

    // print usage
    if (args.count("help")) {
        static constexpr std::size_t MIN_HELP_SIZE = 80;
        if (isatty(STDIN_FILENO)) {
            struct winsize w {};
            if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) != -1) {  // NOLINT
                options.set_width(std::max(static_cast<decltype(w.ws_col)>(MIN_HELP_SIZE), w.ws_col));
            }
        } else {
            options.set_width(MIN_HELP_SIZE);
        }
#ifdef OS_LINUX
        if (isatty(STDIN_FILENO)) {
            struct winsize w {};
            if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) != -1) {  // NOLINT
                static constexpr auto MIN_TTY_SIZE = static_cast<decltype(w.ws_col)>(80);
                options.set_width(std::max(MIN_TTY_SIZE, w.ws_col));
            }
        }
#endif

        std::cout << options.help() << '\n';
        std::cout << '\n';
        std::cout << "The modbus registers are mapped to shared memory objects:" << '\n';
        std::cout << "    type | name                      | mb-server-access | shm name" << '\n';
        std::cout << "    -----|---------------------------|------------------|----------------" << '\n';
        std::cout << "    DO   | Discrete Output Coils     | read-write       | <name-prefix>DO" << '\n';
        std::cout << "    DI   | Discrete Input Coils      | read-only        | <name-prefix>DI" << '\n';
        std::cout << "    AO   | Discrete Output Registers | read-write       | <name-prefix>AO" << '\n';
        std::cout << "    AI   | Discrete Input Registers  | read-only        | <name-prefix>AI" << '\n';
        std::cout << '\n';
        std::cout << "This application uses the following libraries:" << '\n';
        std::cout << "  - cxxopts by jarro2783 (https://github.com/jarro2783/cxxopts)" << '\n';
        std::cout << "  - libmodbus by Stéphane Raimbault (https://github.com/stephane/libmodbus)" << '\n';
        std::cout << "  - cxxshm (https://github.com/NikolasK-source/cxxshm)" << '\n';
        std::cout << "  - cxxsemaphore (https://github.com/NikolasK-source/cxxsemaphore)" << '\n';
        return EX_OK;
    }

    // print version
    if (args.count("shortversion")) {
        std::cout << PROJECT_VERSION << '\n';
        return EX_OK;
    }

    if (args.count("version")) {
        std::cout << PROJECT_NAME << ' ' << PROJECT_VERSION << '\n';
        return EX_OK;
    }

    if (args.count("longversion")) {
        std::cout << PROJECT_NAME << ' ' << PROJECT_VERSION << '\n';
        std::cout << "   compiled with " << COMPILER_INFO << '\n';
        std::cout << "   on system " << SYSTEM_INFO
#ifndef OS_LINUX
                  << "-nonlinux"
#endif
                  << '\n';
        std::cout << "   from git commit " << RCS_HASH << '\n';

        std::cout << "Libraries:\n";

        std::cout << "   " << cxxshm_version_info::NAME << ' ' << cxxshm_version_info::VERSION_STR << '\n';
        std::cout << "      compiled with " << cxxshm_version_info::COMPILER << '\n';
        std::cout << "      on system " << cxxshm_version_info::SYSTEM << '\n';
        std::cout << "      from git commit " << cxxshm_version_info::GIT_HASH << '\n';

        std::cout << "   " << cxxsemaphore_version_info::NAME << ' ' << cxxsemaphore_version_info::VERSION_STR << '\n';
        std::cout << "      compiled with " << cxxsemaphore_version_info::COMPILER << '\n';
        std::cout << "      on system " << cxxsemaphore_version_info::SYSTEM << '\n';
        std::cout << "      from git commit " << cxxsemaphore_version_info::GIT_HASH << '\n';

        std::cout << "   libmodbus " << LIBMODBUS_VERSION_STRING << '\n';

        std::cout << "   cxxopts " << static_cast<int>(cxxopts::version.major) << '.'
                  << static_cast<int>(cxxopts::version.minor) << '.' << static_cast<int>(cxxopts::version.patch)
                  << '\n';

        return EX_OK;
    }

    if (args.count("git-hash")) {
        std::cout << RCS_HASH << '\n';
        return EX_OK;
    }

    // print licenses
    if (args.count("license")) {
        print_licenses(std::cout, false);
        return EX_OK;
    }

    if (args.count("license-full")) {
        print_licenses(std::cout, true);
        return EX_OK;
    }

    // check arguments
    if (args["do-registers"].as<std::size_t>() > MODBUS_MAX_REGS) {
        std::cerr << Print_Time::iso << " ERROR: to many do-registers (maximum: " << MODBUS_MAX_REGS << ")." << '\n';
        return exit_usage();
    }

    if (args["di-registers"].as<std::size_t>() > MODBUS_MAX_REGS) {
        std::cerr << Print_Time::iso << " ERROR: to many di-registers (maximum: " << MODBUS_MAX_REGS << ")." << '\n';
        return exit_usage();
    }

    if (args["ao-registers"].as<std::size_t>() > MODBUS_MAX_REGS) {
        std::cerr << Print_Time::iso << " ERROR: to many ao-registers (maximum: " << MODBUS_MAX_REGS << ")." << '\n';
        return exit_usage();
    }

    if (args["ai-registers"].as<std::size_t>() > MODBUS_MAX_REGS) {
        std::cerr << Print_Time::iso << " ERROR: to many ai-registers (maximum: " << MODBUS_MAX_REGS << ")." << '\n';
        return exit_usage();
    }

    const auto CONNECTIONS = args["connections"].as<std::size_t>();
    if (CONNECTIONS == 0) {
        std::cerr << Print_Time::iso << " ERROR: The number of connections must not be 0" << '\n';
        return exit_usage();
    }

    const auto SEPARATE     = args.count("separate");
    const auto SEPARATE_ALL = args.count("separate-all");
    if (SEPARATE && SEPARATE_ALL) {
        std::cerr << Print_Time::iso << " ERROR: The options --separate and --separate-all cannot be used together."
                  << '\n';
        return EX_USAGE;
    }

    const auto FORCE_SHM = args.count("force") > 0;

    mode_t shm_permissions = DEFAULT_SHM_PERMISSIONS;
    {
        const auto  shm_permissions_str = args["permissions"].as<std::string>();
        bool        fail                = false;
        std::size_t idx                 = 0;
        try {
            shm_permissions = static_cast<mode_t>(std::stoul(shm_permissions_str, &idx, 0));
        } catch (const std::exception &) { fail = true; }
        fail = fail || idx != shm_permissions_str.size();

        static constexpr mode_t INVALID_PERMISSION_BITS_MASK = ~static_cast<mode_t>(0x1FF);
        if (fail || (INVALID_PERMISSION_BITS_MASK & shm_permissions) != 0) {
            std::cerr << Print_Time::iso << " ERROR: Invalid file permissions \"" << shm_permissions_str << '"' << '\n';
            return EX_USAGE;
        }
    }

    // check ulimit

    static constexpr std::size_t NUM_INTERNAL_FILES = 5;       // stderr + stdout + stdin + signal_fd + server socket
    std::size_t min_files = CONNECTIONS + NUM_INTERNAL_FILES;  // number of connections + NUM_INTERNAL_FILES
    if (SEPARATE) min_files += SEPARATE * 4;
    else if (SEPARATE_ALL)
        min_files += Modbus::TCP::Client_Poll::MAX_CLIENT_IDS * 4;
    else
        min_files += 4;
    struct rlimit limit;  // NOLINT
    if (getrlimit(RLIMIT_NOFILE, &limit) != 0) {
        perror("getrlimit");
        return EX_OSERR;
    }

    if (limit.rlim_cur < min_files) {
        std::cerr << Print_Time::iso << " WARNING: limit of open simultaneous files (" << limit.rlim_cur
                  << ") is below the possible maximum that is required for the current settings (" << min_files << ")"
                  << std::endl;  // NOLINT
    }

    // create shared memory object for modbus registers
    std::unique_ptr<Modbus::shm::Shm_Mapping> fallback_mapping;
    if (args.count("separate-all") == 0) {
        try {
            fallback_mapping = std::make_unique<Modbus::shm::Shm_Mapping>(args["do-registers"].as<std::size_t>(),
                                                                          args["di-registers"].as<std::size_t>(),
                                                                          args["ao-registers"].as<std::size_t>(),
                                                                          args["ai-registers"].as<std::size_t>(),
                                                                          args["name-prefix"].as<std::string>(),
                                                                          FORCE_SHM,
                                                                          shm_permissions);
        } catch (const std::system_error &e) {
            std::cerr << Print_Time::iso << " ERROR: " << e.what() << '\n';
            return EX_OSERR;
        }
    }

    std::array<modbus_mapping_t *, Modbus::TCP::Client_Poll::MAX_CLIENT_IDS> mb_mappings {};
    std::vector<std::unique_ptr<Modbus::shm::Shm_Mapping>>                   separate_mappings;

    if (SEPARATE_ALL) {
        for (std::size_t i = 0; i < Modbus::TCP::Client_Poll::MAX_CLIENT_IDS; ++i) {
            std::ostringstream sstr;
            sstr << args["name-prefix"].as<std::string>() << std::setfill('0') << std::hex << std::setw(2) << i << '_';

            try {
                separate_mappings.emplace_back(
                        std::make_unique<Modbus::shm::Shm_Mapping>(args["do-registers"].as<std::size_t>(),
                                                                   args["di-registers"].as<std::size_t>(),
                                                                   args["ao-registers"].as<std::size_t>(),
                                                                   args["ai-registers"].as<std::size_t>(),
                                                                   sstr.str(),
                                                                   FORCE_SHM,
                                                                   shm_permissions));
                mb_mappings[i] = separate_mappings.back()->get_mapping();  // NOLINT
            } catch (const std::system_error &e) {
                std::cerr << Print_Time::iso << " ERROR: " << e.what() << '\n';
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
                                                                   FORCE_SHM,
                                                                   shm_permissions));
                mb_mappings[a] = separate_mappings.back()->get_mapping();  // NOLINT
            } catch (const std::system_error &e) {
                std::cerr << Print_Time::iso << " ERROR: " << e.what() << '\n';
                return EX_OSERR;
            }
        }
    }


    // create modbus client
    std::unique_ptr<Modbus::TCP::Client_Poll> client;
    try {
        client = std::make_unique<Modbus::TCP::Client_Poll>(args["host"].as<std::string>(),
                                                            args["service"].as<std::string>(),
                                                            mb_mappings,
#ifdef OS_LINUX
                                                            args["tcp-timeout"].as<std::size_t>(),
#else
                                                            0,
#endif
                                                            CONNECTIONS);
        client->set_debug(args.count("monitor"));
    } catch (const std::runtime_error &e) {
        std::cerr << Print_Time::iso << " ERROR: " << e.what() << '\n';
        return EX_SOFTWARE;
    }

    // set timeouts if required
    try {
        if (args.count("response-timeout")) { client->set_response_timeout(args["response-timeout"].as<double>()); }

        if (args.count("byte-timeout")) { client->set_byte_timeout(args["byte-timeout"].as<double>()); }
    } catch (const std::runtime_error &e) {
        std::cerr << Print_Time::iso << " ERROR: " << e.what() << '\n';
        return EX_SOFTWARE;
    }

    // add semaphore if required
    try {
        if (args.count("semaphore")) {
            client->enable_semaphore(args["semaphore"].as<std::string>(), args.count("semaphore-force"));
        }
    } catch (const std::system_error &e) {
        std::cerr << Print_Time::iso << " ERROR: " << e.what() << '\n';
        return EX_SOFTWARE;
    }

    auto RECONNECT = args.count("reconnect") != 0;

    std::cerr << Print_Time::iso << " INFO: Listening on " << client->get_listen_addr() << " for connections."
              << std::endl;  // NOLINT

    try {
        [&]() {
            while (true) {
                auto ret = client->run(signal_fd, RECONNECT, -1);

                switch (ret) {
                    case Modbus::TCP::Client_Poll::run_t::ok: continue;
                    case Modbus::TCP::Client_Poll::run_t::semaphore:
                    case Modbus::TCP::Client_Poll::run_t::term_signal: return;
                    case Modbus::TCP::Client_Poll::run_t::term_nocon:
                        std::cerr << Print_Time::iso << " INFO: No more active connections." << std::endl;  // NOLINT
                        return;
                    case Modbus::TCP::Client_Poll::run_t::timeout:
                    case Modbus::TCP::Client_Poll::run_t::interrupted: continue;
                }
            }
        }();
    } catch (const std::exception &e) {
        if (!terminate) std::cerr << Print_Time::iso << " ERROR: " << e.what() << std::endl;  // NOLINT
    }

    std::cerr << Print_Time::iso << " INFO: Terminating...\n";
}
