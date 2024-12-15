# README

## Environment
Tested on Oracle Virtualbox running Ubuntu 24.04.1 LTS

**gcc**: Ubuntu 13.3.0-6ubuntu2~24.04) 13.3.0

**python3**: Python 3.12.3

## How to Compile
The unit under test *Cpp code* is compiled into a shared library file so that it is callable from the *python* test code

Generate **.so** file with the following:
```
gcc -fPIC -Wall uart_data_repeater.cpp imu_parser.cpp -lpthread -shared -Wl,-soname,libuut.so -o libuut.so -DDEBUG
```
## How to Run
Python3 CodeTester.py

> [!NOTE]
>
> This code requires access to pseudoterminals and serial devices. **root** access may be necessary.
