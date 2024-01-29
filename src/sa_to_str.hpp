/*
 * Copyright (C) 2022 Nikolas Koesling <nikolas@koesling.info>.
 * This program is free software. You can redistribute it and/or modify it under the terms of the GPLv3 License.
 */

#pragma once

#include <string>
#include <sys/socket.h>

/**
 * @brief convert socket address to string
 * @param sa socket address
 * @return sa as string
 */
std::string sockaddr_to_str(const sockaddr_storage &sa);
