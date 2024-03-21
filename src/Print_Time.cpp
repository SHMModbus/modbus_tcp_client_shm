/*
 * Copyright (C) 2022 Nikolas Koesling <nikolas@koesling.info>.
 * This program is free software. You can redistribute it and/or modify it under the terms of the GPLv3 License.
 */

#include "Print_Time.hpp"

#include <array>
#include <ctime>

Print_Time Print_Time::iso("%F_%T");

std::ostream &operator<<(std::ostream &o, const Print_Time &p) {
    auto                                            now = time(nullptr);
    std::array<char, sizeof "1234-25-78T90:12:34Z"> buf {};
    strftime(buf.data(), buf.size(), p.format.c_str(), gmtime(&now));
    o << buf.data();
    return o;
}
