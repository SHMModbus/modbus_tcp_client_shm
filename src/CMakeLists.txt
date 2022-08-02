# ---------------------------------------- source files (*.cpp, *.cc, ...) ---------------------------------------------
# ======================================================================================================================

target_sources(${Target} PRIVATE main.cpp)
target_sources(${Target} PRIVATE modbus_shm.cpp)
target_sources(${Target} PRIVATE Modbus_TCP_Slave.cpp)
target_sources(${Target} PRIVATE license.cpp)


# ---------------------------------------- header files (*.jpp, *.h, ...) ----------------------------------------------
# ======================================================================================================================
target_sources(${Target} PRIVATE modbus_shm.hpp)
target_sources(${Target} PRIVATE Modbus_TCP_Slave.hpp)
target_sources(${Target} PRIVATE license.hpp)



# ---------------------------------------- subdirectories --------------------------------------------------------------
# ======================================================================================================================
