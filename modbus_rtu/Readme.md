## README

### Compilation of the library
```
unzip libmodbus-3.1.11.zip # Unzip the package
cd libmodbus-3.1.11 # Enter the unzipped directory
./autogen.sh # Generate configuration
./configure --host=aarch64 # Generate Makefile
make clean # Build
make
```

If the compilation has been done successfully, the library files (.a, .la, .lai) is expected to be in the folder ``libmodbus-3.1.11/src/.libs/``.
Please copy these libraray files to folder ``libmodbus`` when ``libmodbus-3.1.11.zip`` is updated (e.g., 3.1.12). The linker will always link library files in ``libmodbus`` (instead of ``libmodbus-3.1.11/src/.libs/``).

N.B.:
If the compilation reports error as followings:
```
In file included from unit-test-server.c:27:0:
unit-test.h:34:57: error: initializer element is not constant
 const uint16_t UT_BITS_ADDRESS_INVALID_REQUEST_LENGTH = UT_BITS_ADDRESS + 2;// 0x130 + 2;//UT_BITS_ADDRESS + 2;
                                                         ^~~~~~~~~~~~~~~
Makefile:681: recipe for target 'unit-test-server.o' failed
```
A minor modification on ``tests/unit-test.h`` can avoid the error.
```
const uint16_t UT_BITS_ADDRESS_INVALID_REQUEST_LENGTH = 0x130 + 2;//UT_BITS_ADDRESS + 2;
```

### Usage of modbus register remapping
The modbus feature supports remapping the registers to new ones to adapt with a legacy master device. For instance, the remapped register addresses are expected to be within 0 - 9999. The original registers at 2, 9, and 4 are expected to be remapped to 0, 4, and 2. Then a magic file as followings can achieve this remapping. 
```
{
    ...

    "modbus_mapping": {
        "offset": [10000],
        "mappingNum": [3],
        "src": [
            2,
            9,
            4
        ],
        "dest": [
            0,
            4,
            2
        ]
    }

    ...
}
```
The original register address would be moved by adding an ``offset`` first (e.g., the original register address 2 would be moved to 10002). The element numbers of ``src`` and ``dest`` should be identical and defined by ``mappingNum`` (no more than 1024 in current implementation). The addresses in the ``src`` and ``dest`` can be inconsecutive and repeated (if necessary).