# ---------------------------------------- source files (*.cpp, *.cc, ...) ---------------------------------------------
# ======================================================================================================================

target_sources(${Target} PRIVATE main.cpp)
target_sources(${Target} PRIVATE modbus_shm.cpp)
target_sources(${Target} PRIVATE Modbus_TCP_Client_poll.cpp)
target_sources(${Target} PRIVATE license.cpp)
target_sources(${Target} PRIVATE sa_to_str.cpp)


# ---------------------------------------- header files (*.jpp, *.h, ...) ----------------------------------------------
# ======================================================================================================================
target_sources(${Target} PRIVATE modbus_shm.hpp)
target_sources(${Target} PRIVATE Modbus_TCP_Client_poll.hpp)
target_sources(${Target} PRIVATE license.hpp)
target_sources(${Target} PRIVATE sa_to_str.hpp)



# ---------------------------------------- subdirectories --------------------------------------------------------------
# ======================================================================================================================
