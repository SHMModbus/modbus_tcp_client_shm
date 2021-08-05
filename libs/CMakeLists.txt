
# ---------------------------------------- subdirectories --------------------------------------------------------------
# ======================================================================================================================
include(../modbus.cmake)
target_link_libraries(${Target} PRIVATE modbus)

# ---------------------------------------- link libraries --------------------------------------------------------------
# ======================================================================================================================
target_link_libraries(${Target} PRIVATE rt)
target_link_libraries(${Target} PRIVATE stdc++fs)

add_subdirectory(cxxopts EXCLUDE_FROM_ALL)
target_link_libraries(${Target} PRIVATE cxxopts)
