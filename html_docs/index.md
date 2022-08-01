# Shared Memory Modbus TCP Client

This project is a simple command line based Modbus TCP client for POXIX compatible operating systems that stores the contents of its registers in shared memory.

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
