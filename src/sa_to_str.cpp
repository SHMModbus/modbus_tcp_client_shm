/*
 * Copyright (C) 2022 Nikolas Koesling <nikolas@koesling.info>.
 * This program is free software. You can redistribute it and/or modify it under the terms of the MIT License.
 */

#include "sa_to_str.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sstream>

std::string sockaddr_to_str(const sockaddr_storage &sa) {
    char buffer[INET6_ADDRSTRLEN + 1];
    if (sa.ss_family == AF_INET) {
        auto peer_in = reinterpret_cast<const struct sockaddr_in *>(&sa);
        inet_ntop(sa.ss_family, &peer_in->sin_addr, buffer, sizeof(buffer));
        std::ostringstream sstr;
        return buffer;
    } else if (sa.ss_family == AF_INET6) {
        auto peer_in6 = reinterpret_cast<const struct sockaddr_in6 *>(&sa);
        inet_ntop(sa.ss_family, &peer_in6->sin6_addr, buffer, sizeof(buffer));
        std::ostringstream sstr;
        sstr << '[' << buffer << ']';
        return sstr.str();
    } else {
        return "UNKNOWN";
    }
}