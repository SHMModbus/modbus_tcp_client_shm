/*
 * Copyright (C) 2024 Nikolas Koesling <nikolas@koesling.info>.
 * This program is free software. You can redistribute it and/or modify it under the terms of the GPLv3 License.
 */

#include "Mb_Proc_Signal.hpp"

#include <cerrno>
#include <modbus/modbus.h>
#include <system_error>

Mb_Proc_Signal Mb_Proc_Signal::instance;  // NOLINT

Mb_Proc_Signal &Mb_Proc_Signal::get_instance() {
    return instance;
}

void Mb_Proc_Signal::add_process(pid_t process) {
    processes.insert(process);
}

void Mb_Proc_Signal::send_signal() {
    for (auto proc : processes) {
        auto ret = kill(proc, SIGUSR1);
        if (ret == -1) {
            throw std::system_error(
                    errno, std::generic_category(), "Failed to send signal to process " + std::to_string(proc));
        }
    }
}

void mb_callback(uint8_t mb_funtion_code) {
    switch (mb_funtion_code) {
        case MODBUS_FC_WRITE_SINGLE_COIL:
        case MODBUS_FC_WRITE_SINGLE_REGISTER:
        case MODBUS_FC_WRITE_MULTIPLE_COILS:
        case MODBUS_FC_WRITE_MULTIPLE_REGISTERS:
        case MODBUS_FC_WRITE_AND_READ_REGISTERS: Mb_Proc_Signal::get_instance().send_signal(); break;
        default:
            // do nothing
            break;
    }
}
