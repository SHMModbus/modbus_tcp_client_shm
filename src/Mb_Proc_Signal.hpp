/*
 * Copyright (C) 2024 Nikolas Koesling <nikolas@koesling.info>.
 * This program is free software. You can redistribute it and/or modify it under the terms of the GPLv3 License.
 */

#include <cstdint>
#include <unistd.h>
#include <unordered_set>

class Mb_Proc_Signal final {
private:
    std::unordered_set<pid_t> processes;

    Mb_Proc_Signal() = default;

    static Mb_Proc_Signal instance;

public:
    Mb_Proc_Signal(const Mb_Proc_Signal &)            = delete;
    Mb_Proc_Signal(Mb_Proc_Signal &&)                 = delete;
    Mb_Proc_Signal &operator=(const Mb_Proc_Signal &) = delete;
    Mb_Proc_Signal &operator=(Mb_Proc_Signal &&)      = delete;
    ~Mb_Proc_Signal()                                 = default;

    static Mb_Proc_Signal &get_instance();

    void add_process(pid_t process);

    void send_signal(const union sigval &value);
};

void mb_callback(uint8_t mb_funtion_code);
