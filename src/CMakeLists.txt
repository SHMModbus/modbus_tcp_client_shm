# ---------------------------------------- source files (*.cpp, *.cc, ...) ---------------------------------------------
# ======================================================================================================================

target_sources(${Target} PRIVATE main.cpp)
target_sources(${Target} PRIVATE modbus_shm.cpp)
target_sources(${Target} PRIVATE Modbus_TCP_Client_poll.cpp)
target_sources(${Target} PRIVATE license.cpp)
target_sources(${Target} PRIVATE sa_to_str.cpp)
target_sources(${Target} PRIVATE Print_Time.cpp)
target_sources(${Target} PRIVATE Mb_Proc_Signal.cpp)

# ---------------------------------------- subdirectories --------------------------------------------------------------
# ======================================================================================================================

add_subdirectory(generated)
