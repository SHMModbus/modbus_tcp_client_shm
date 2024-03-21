/*
 * Copyright (C) 2021-2022 Nikolas Koesling <nikolas@koesling.info>.
 * This program is free software. You can redistribute it and/or modify it under the terms of the GPLv3 License.
 */

#include "modbus_shm.hpp"

#include <stdexcept>

namespace Modbus::shm {

static constexpr std::size_t MAX_MODBUS_REGISTERS = 0x10000;

Shm_Mapping::Shm_Mapping(std::size_t        nb_bits,             // NOLINT
                         std::size_t        nb_input_bits,       // NOLINT
                         std::size_t        nb_registers,        // NOLINT
                         std::size_t        nb_input_registers,  // NOLINT
                         const std::string &prefix,
                         bool               force,
                         mode_t             permissions) {
    // check argument ranges
    if (nb_bits > MAX_MODBUS_REGISTERS || !nb_bits)
        throw std::invalid_argument("invalid number of digital output registers.");
    if (nb_input_bits > MAX_MODBUS_REGISTERS || !nb_input_bits)
        throw std::invalid_argument("invalid number of digital input registers.");
    if (nb_registers > MAX_MODBUS_REGISTERS || !nb_registers)
        throw std::invalid_argument("invalid number of analog output registers.");
    if (nb_input_registers > MAX_MODBUS_REGISTERS || !nb_input_registers)
        throw std::invalid_argument("invalid number of analog input registers.");

    // set register count
    mapping.nb_bits            = static_cast<int>(nb_bits);
    mapping.nb_input_bits      = static_cast<int>(nb_input_bits);
    mapping.nb_registers       = static_cast<int>(nb_registers);
    mapping.nb_input_registers = static_cast<int>(nb_input_registers);

    // create shm objects
    shm_data[DO] = std::make_unique<cxxshm::SharedMemory>(prefix + "DO", nb_bits, false, !force, permissions);
    shm_data[DI] = std::make_unique<cxxshm::SharedMemory>(prefix + "DI", nb_input_bits, false, !force, permissions);
    shm_data[AO] = std::make_unique<cxxshm::SharedMemory>(prefix + "AO", 2 * nb_registers, false, !force, permissions);
    shm_data[AI] =
            std::make_unique<cxxshm::SharedMemory>(prefix + "AI", 2 * nb_input_registers, false, !force, permissions);

    // set shm objects as modbus register storage
    mapping.tab_bits            = static_cast<uint8_t *>(shm_data[DO]->get_addr());
    mapping.tab_input_bits      = static_cast<uint8_t *>(shm_data[DI]->get_addr());
    mapping.tab_registers       = static_cast<uint16_t *>(shm_data[AO]->get_addr());
    mapping.tab_input_registers = static_cast<uint16_t *>(shm_data[AI]->get_addr());
}

}  // namespace Modbus::shm
