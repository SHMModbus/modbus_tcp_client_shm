/*
 * Copyright (C) 2024 Nikolas Koesling <nikolas@koesling.info>.
 * This program is free software. You can redistribute it and/or modify it under the terms of the GPLv3 License.
 */

#include "Mb_Proc_Signal.hpp"
#include "Print_Time.hpp"

#include <cerrno>
#include <format>
#include <iostream>
#include <modbus/modbus.h>
#include <system_error>
#include <vector>

Mb_Proc_Signal Mb_Proc_Signal::instance;  // NOLINT

Mb_Proc_Signal &Mb_Proc_Signal::get_instance() {
    return instance;
}

void Mb_Proc_Signal::add_process(pid_t process) {
    auto ret = kill(process, 0);
    if (ret == -1) {
        if (errno == ESRCH) { throw std::runtime_error(std::format("no such process: {}", process)); }
        throw std::system_error(
                errno, std::generic_category(), std::format("Failed to send signal to process {}", process));
    }
    processes.insert(process);
}

void Mb_Proc_Signal::send_signal(const union sigval &value) {
    std::vector<pid_t> erased;
    for (auto proc : processes) {
        auto ret = sigqueue(proc, SIGUSR1, value);
        if (ret == -1) {
            if (errno == ESRCH) {
                erased.emplace_back(proc);
            } else {
                throw std::system_error(
                        errno, std::generic_category(), std::format("Failed to send signal to process {}", proc));
            }
        }
    }

    for (auto proc : erased) {
        std::cerr << Print_Time::iso << " WARNING: process " << proc
                  << " does no longer exist. Removing from SIGUSR1 receivers.\n";
        processes.erase(proc);
    }
}

void mb_callback(uint8_t mb_funtion_code) {
    switch (mb_funtion_code) {
        case MODBUS_FC_WRITE_SINGLE_COIL:
        case MODBUS_FC_WRITE_SINGLE_REGISTER:
        case MODBUS_FC_WRITE_MULTIPLE_COILS:
        case MODBUS_FC_WRITE_MULTIPLE_REGISTERS:
        case MODBUS_FC_WRITE_AND_READ_REGISTERS:
            Mb_Proc_Signal::get_instance().send_signal({.sival_int = mb_funtion_code});
            break;
        default:
            // do nothing
            break;
    }
}
