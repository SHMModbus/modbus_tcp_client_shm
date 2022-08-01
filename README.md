# Modbus TCP client shm

Modbus tcp client that stores its data (registers) in shared memory objects.

## Build
```
git submodule init
git submodule update
mkdir build
cd build
cmake .. -DCMAKE_CXX_COMPILER=$(which clang++) -DCMAKE_BUILD_TYPE=Release -DCLANG_FORMAT=OFF -DCOMPILER_WARNINGS=OFF
cmake --build .
```

As an alternative to the ```git submodule``` commands, the ```--recursive``` option can be used with ```git clone```.

## Use
```
Modbus_TCP_client_shm [OPTION...]

  -i, --ip arg            ip to listen for incoming connections (default: 0.0.0.0)
  -p, --port arg          port to listen for incoming connections (default: 502)
  -n, --name-prefix arg   shared memory name prefix (default: modbus_)
      --do-registers arg  number of digital output registers (default: 65536)
      --di-registers arg  number of digital input registers (default: 65536)
      --ao-registers arg  number of analog output registers (default: 65536)
      --ai-registers arg  number of analog input registers (default: 65536)
  -m, --monitor           output all incoming and outgoing packets to stdout
  -r, --reconnect         do not terminate if Master disconnects.
  -h, --help              print usage


The modbus registers are mapped to shared memory objects:
    type | name                      | master-access   | shm name
    -----|---------------------------|-----------------|----------------
    DO   | Discrete Output Coils     | read-write      | <name-prefix>DO
    DI   | Discrete Input Coils      | read-only       | <name-prefix>DI
    AO   | Discrete Output Registers | read-write      | <name-prefix>AO
    AI   | Discrete Input Registers  | read-only       | <name-prefix>AI
```

### Use privileged ports
The standard modbus port (502) can be used only by the root user under linux by default. 
To circumvent this, you can create an entry in the iptables that redirects packets on the standard modbus port to a higher port.
The following example redirects packets from port 502 (standard modbus port) to port 5020
```
iptables -A PREROUTING -t nat -p tcp --dport 502 -j REDIRECT --to-port 5020
```
The modbus client must be called with the option ```-p 5020``` 

## Libraries
This application uses the following libraries:
- cxxopts by jarro2783 (https://github.com/jarro2783/cxxopts)
- libmodbus by Stéphane Raimbault (https://github.com/stephane/libmodbus)
