#
# Builds the modbus library
#
# Outputs the following target:
#   modbus
#
include(ExternalProject)
set(MODBUS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libmodbus)
set(MODBUS_BIN ${CMAKE_CURRENT_BINARY_DIR}/libmodbus)
set(MODBUS_STATIC_LIB ${MODBUS_BIN}/lib/libmodbus.a)
set(MODBUS_INCLUDES ${MODBUS_BIN}/include)

file(MAKE_DIRECTORY ${MODBUS_INCLUDES})

ExternalProject_Add(
        libmodbus
        PREFIX ${MODBUS_BIN}
        SOURCE_DIR ${MODBUS_DIR}
        DOWNLOAD_COMMAND cd ${MODBUS_DIR} && git clean -dfX
        CONFIGURE_COMMAND cd ${MODBUS_DIR} && ${MODBUS_DIR}/autogen.sh && ${MODBUS_DIR}/configure --srcdir=${MODBUS_DIR} --prefix=${MODBUS_BIN} --enable-static=yes --disable-shared
        BUILD_COMMAND make
        INSTALL_COMMAND make install
        BUILD_BYPRODUCTS ${MODBUS_STATIC_LIB}
)

add_library(modbus STATIC IMPORTED GLOBAL)

add_dependencies(modbus libmodbus)

set_target_properties(modbus PROPERTIES IMPORTED_LOCATION ${MODBUS_STATIC_LIB})
set_target_properties(modbus PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${MODBUS_INCLUDES})
