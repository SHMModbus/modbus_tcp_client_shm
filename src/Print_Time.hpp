/*
 * Copyright (C) 2022 Nikolas Koesling <nikolas@koesling.info>.
 * This program is free software. You can redistribute it and/or modify it under the terms of the GPLv3 License.
 */

#pragma once

#include <ostream>
#include <string>

class Print_Time {
public:
    static Print_Time iso;

private:
    std::string format;

public:
    explicit Print_Time(std::string format) : format(std::move(format)) {}

    friend std::ostream &operator<<(std::ostream &o, const Print_Time &p);
};
