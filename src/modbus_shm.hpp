/*
 * Copyright (C) 2021-2022 Nikolas Koesling <nikolas@koesling.info>.
 * This program is free software. You can redistribute it and/or modify it under the terms of the MIT License.
 */

#pragma once

#include "cxxshm.hpp"
#include "modbus/modbus.h"
#include <array>
#include <cstddef>
#include <memory>
#include <string>

namespace Modbus {
namespace shm {

/*! \brief class that creates a modbus_mapping_t object that uses shared memory (shm) objects.
 *
 * All required shm objects are created on construction and and deleted on destruction.
 */
class Shm_Mapping final {
private:
    enum reg_index_t : std::size_t { DO, DI, AO, AI, REG_COUNT };

    //! data for a shared memory object
    struct shm_data_t {
        std::string name = std::string();  //!< name of the object
        int         fd   = -1;             //!< file descriptor
        std::size_t size;                  //!< size in bytes
        void       *addr = nullptr;        //!< mapped address
    };

    //! modbus lib storage object
    modbus_mapping_t mapping {};

    //! info for all shared memory objects
    std::array<std::unique_ptr<cxxshm::SharedMemory>, reg_index_t::REG_COUNT> shm_data;

public:
    /*! \brief creates a new modbus_mapping_t. Like modbus_mapping_new(), but creates shared memory objects to store its
     * data.
     *
     * this function creates 4 shared memory objects:
     *      - <shm_name_prefix>DO
     *      - <shm_name_prefix>DI
     *      - <shm_name_prefix>AO
     *      - <shm_name_prefix>AI
     *
     * @param nb_bits number of digital output registers (DO)
     * @param nb_input_bits number of digital input registers (DI)
     * @param nb_registers number of analog output registers (AO)
     * @param nb_input_registers number of analog input registers (AI)
     * @param shm_name_prefix name prefix of the created shared memory object
     * @param force do not fail if the shared memory exist, but use the existing shared memory
     */
    Shm_Mapping(std::size_t        nb_bits,
                std::size_t        nb_input_bits,
                std::size_t        nb_registers,
                std::size_t        nb_input_registers,
                const std::string &shm_name_prefix = "modbus_",
                bool               force           = false);

    ~Shm_Mapping() = default;

    /*! \brief get a pointer to the created modbus_mapping_t object
     *
     * @return pointer to modbus_mapping_t object
     */
    modbus_mapping_t *get_mapping() { return &mapping; }
};

}  // namespace shm
}  // namespace Modbus
