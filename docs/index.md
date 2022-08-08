# Shared Memory Modbus TCP Client

This project is a simple command line based Modbus TCP client for POSIX compatible operating systems that stores the contents of its registers in shared memory.


## Basic operating principle

The client creates four shared memories. 
One for each register type:
- Discrete Output Coils (DO)
- Discrete Input Coils (DI)
- Discrete Output Registers (AO)
- Discrete Input Registers (AI)

All registers are initialized with 0 at the beginning.
The Modbus master reads and writes directly the values from these shared memories.

The actual functionality of the client is realized by applications that read data from or write data to the shared memory.


## Use the Application
The application can be started completely without command line arguments. 
In this case the client listens for connections on all IPs on port 502 (the default modbus port).
The application terminates if the master disconnects.

The arguments ```--port``` and ```--ip``` can be used to specify port and ip to listen to.

By using the command line argument ```--monitor``` all incoming and outgoing packets are printed on stdout. 
This option should be used carefully, as it generates large amounts of output depending on the masters polling cycle and the number of used registers.

The ```--reconnect``` option can be used to specify that the application is not terminated when the master disconnects, but waits for a new connection.

The client creates four shared memories and names them ```modbus_DO```, ```modbus_DI```, ```modbus_AO``` and ```modbus_AI``` by default.
The prefix modbus_ can be changed via the argument ```--name-prefix```.
The suffixes for the register type (DO, DI, AO, AI) cannot be changed and will always be appended to the prefix.

By default, the client starts with the maximum possible number of modbus registers (65536 per register type).
The number of registers can be changed using the ```--xx-registers``` (replace xx with the register type) command line arguments.

### Examples
Start client and listen to all incoming connections on the modbus standard port:
```
modbus-tcp-client-shm
```

Start client and listen to all incoming connections on port 10000 and wait for a new connection if the connection is lost:
```
modbus-tcp-client-shm -p 10000 -r
```

Start client and listen to incoming connections on ip 127.0.0.1 on port 10000:
```
modbus-tcp-client-shm -p 10000 -i 127.0.0.1
```

### Use privileged ports
Ports below 1024 cannot be used by standard users.
Therefore, the default modbus port (502) cannot be used without further action.

Here are two ways to use the port anyway:
#### iptables (recommended)
An entry can be added to the iptables that forwards the packets on the actual port to a higher port.

The following example redirects all tcp packets on port 502 to port 5020:
```
iptables -A PREROUTING -t nat -p tcp --dport 502 -j REDIRECT --to-port 5020
```

#### setcap
The command ```setcap``` can be used to allow the application to access privileged ports.
However, this option gives the application significantly more rights than it actually needs and should therefore be avoided.

This option cannot be used with flatpaks.

```
setcap 'cap_net_bind_service=+ep' /path/to/binary
```


## Using the Flatpak package
The flatpak package can be installed via the .flatpak file.
This can be downloaded from the GitHub [projects release page](https://github.com/NikolasK-source/modbus_tcp_client_shm/releases):

```
flatpak install --user modbus-tcp-client-shm.flatpak
```

The application is then executed as follows:
```
flatpak run network.koesling.modbus-tcp-client-shm
```

To enable calling with ```modbus-tcp-client-shm``` [this script](https://gist.github.com/NikolasK-source/f0ef53fe4be7922901a1543e3cce8a97) can be used.
In order to be executable everywhere, the path in which the script is placed must be in the ```PATH``` environment variable.


## Build from Source

The following packages are required for building the application:
- cmake
- clang or gcc

Additionally, the following packages are required to build the modbus library:
- autoconf 
- automake 
- libtool


Use the following commands to build the application:
```
git clone --recursive https://github.com/NikolasK-source/modbus_tcp_client_shm.git
cd modbus_tcp_client_shm
mkdir build
cmake -B build . -DCMAKE_BUILD_TYPE=Release -DCLANG_FORMAT=OFF -DCOMPILER_WARNINGS=OFF
cmake --build build
```

The binary is located in the build directory.


## Links to related projects

### General Shared Memory Tools
- [Shared Memory Dump](https://nikolask-source.github.io/dump_shm/)
- [Shared Memory Write](https://nikolask-source.github.io/write_shm/)
- [Shared Memory Random](https://nikolask-source.github.io/shared_mem_random/)

### Modbus Clients
- [RTU](https://nikolask-source.github.io/modbus_rtu_client_shm/)
- [TCP](https://nikolask-source.github.io/modbus_tcp_client_shm/)

### Modbus Shared Memory Tools
- [STDIN to Modbus](https://nikolask-source.github.io/stdin_to_modbus_shm/)
- [Float converter](https://nikolask-source.github.io/modbus_conv_float/)


## License

MIT License

Copyright (c) 2021-2022 Nikolas Koesling

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
