#include "modbus_shm.hpp"

#include <fcntl.h>
#include <iostream>
#include <stdexcept>
#include <sys/mman.h>
#include <system_error>
#include <unistd.h>

namespace Modbus {
namespace shm {

Shm_Mapping::Shm_Mapping(std::size_t        nb_bits,
                         std::size_t        nb_input_bits,
                         std::size_t        nb_registers,
                         std::size_t        nb_input_registers,
                         const std::string &prefix) {
    // check argument ranges
    if (nb_bits > 0x10000 || !nb_bits) throw std::invalid_argument("invalid number of digital output registers.");
    if (nb_input_bits > 0x10000 || !nb_input_bits)
        throw std::invalid_argument("invalid number of digital input registers.");
    if (nb_registers > 0x10000 || !nb_registers)
        throw std::invalid_argument("invalid number of analog output registers.");
    if (nb_input_registers > 0x10000 || !nb_input_registers)
        throw std::invalid_argument("invalid number of analog input registers.");

    // set register count
    mapping.nb_bits            = static_cast<int>(nb_bits);
    mapping.nb_input_bits      = static_cast<int>(nb_input_bits);
    mapping.nb_registers       = static_cast<int>(nb_registers);
    mapping.nb_input_registers = static_cast<int>(nb_input_registers);

    // calculate shm object size
    shm_data[DO].size = nb_bits;
    shm_data[DI].size = nb_input_bits;
    shm_data[AO].size = nb_registers * 2;
    shm_data[AI].size = nb_input_registers * 2;

    // create shm object names
    shm_data[DO].name = prefix + "DO";
    shm_data[DI].name = prefix + "DI";
    shm_data[AO].name = prefix + "AO";
    shm_data[AI].name = prefix + "AI";

    // create and map shm objects
    for (std::size_t i = 0; i < reg_index_t::REG_COUNT; ++i) {
        auto &shm = shm_data[i];

        // create shm object
        shm.fd = shm_open(shm.name.c_str(), O_RDWR | O_CREAT, 0660);
        if (shm.fd < 0) {
            throw std::system_error(
                    errno, std::generic_category(), "Failed to create shared memory '" + shm.name + '\'');
        }

        // set size of shm object
        if (ftruncate(shm.fd, static_cast<__off_t>(shm.size))) {
            throw std::system_error(
                    errno, std::generic_category(), "Failed to resize shared memory '" + shm.name + '\'');
        }

        // map shm object
        shm.addr = mmap(nullptr, shm.size, PROT_WRITE | PROT_READ, MAP_SHARED, shm.fd, 0);
        if (shm.addr == nullptr && shm.addr == MAP_FAILED) {
            shm.addr = nullptr;
            throw std::system_error(errno, std::generic_category(), "Failed to map shared memory '" + shm.name + '\'');
        }
    }

    // set shm objects as modbus register storage
    mapping.tab_bits            = static_cast<uint8_t *>(shm_data[DO].addr);
    mapping.tab_input_bits      = static_cast<uint8_t *>(shm_data[DI].addr);
    mapping.tab_registers       = static_cast<uint16_t *>(shm_data[AO].addr);
    mapping.tab_input_registers = static_cast<uint16_t *>(shm_data[AI].addr);
}

Shm_Mapping::~Shm_Mapping() {
    // unmap and delete shm objects
    for (std::size_t i = 0; i < reg_index_t::REG_COUNT; ++i) {
        auto &shm = shm_data[i];
        if (shm.addr) {
            if (munmap(shm.addr, shm.size)) { perror(("Failed to unmap shared memory '" + shm.name + '\'').c_str()); }
        }

        if (shm.fd != -1) {
            if (close(shm.fd)) {
                perror(("Failed to close shared memory file descriptor '" + shm.name + '\'').c_str());
            }

            if (shm_unlink(shm.name.c_str())) {
                perror(("Failed to unlink shared memory '" + shm.name + '\'').c_str());
            }
        }
    }
}

}  // namespace shm
}  // namespace Modbus
