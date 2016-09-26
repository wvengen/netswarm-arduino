NetSwarm
========

NetSwarm is an [Arduino](http://www.arduino.org/) library for creating a network
of interacting devices, without the need for a master. Think of it as a simple
peer-to-peer network; a swarm of devices acting collectively.

How does it work? All devices are connected to a [Modbus](https://en.wikipedia.org/wiki/Modbus)
network and act as a server, which means they respond to requests for reading
and writing registers. Devices may also act as a master and send updates over
the network, either to a specific device or to all devices. This allows devices
to send updates and respond to each other.

Features:
- TCP/IP or UDP/IP networking (on various hardware)
- Modbus protocol (using [modbus-arduino](https://github.com/wvengen/modbus-arduino))
- Registers can persist in EEPROM
- Broadcast support when using UDP/IP
- A [monitoring and debugging webapp](https://github.com/wvengen/netswarm-webapp)


Getting started
---------------

You'll need
* An [Arduino device](https://www.arduino.cc/en/Main/Products) + Ethernet Shield
  (like [1](http://www.arduino.org/learning/reference/Ethernet-Library) or
   [2](http://www.arduino.org/learning/reference/Ethernet-two-Library))
  or integrated (like
   [Leonardo Eth](http://www.arduino.org/products/boards/arduino-leonardo-eth) or
   [EtherTen](https://github.com/freetronics/EtherTen)).
* An Arduino [development environment](https://www.arduino.cc/en/Main/Software)
* A network cable (to connect the device to your computer)
* to have the [modbus-arduino library](https://github.com/wvengen/modbus-arduino/releases)
  installed ([read how](https://www.arduino.cc/en/Guide/Libraries#toc4))
* to have the NetSwarm library installed (see [Releases](https://github.com/wvengen/netswarm-arduino/releases), or _Development_ below)

Then start a new sketch, a minimal NetSwarm-based program looks like this:

```cpp
#include <SPI.h>
#include <EEPROM.h>
#include <Ethernet.h>
#include <Modbus.h>
#include <ModbusIP.h>
#include <NetSwarm.h>

NetSwarm<ModbusIP> ns;

void setup() {
  ns.config();
}

void loop() {
  ns.task();
}
```

First we need to include some dependencies for NetSwarm, as well as NetSwarm
itself.

Then we instantiate `NetSwarm` for use with the `ModbusIP` library. The syntax
may be unfamiliar to you (it uses C++ templates), but `NetSwarm<ModbusIP>`
essentially means that you want to have a `NetSwarm` instance that uses
`ModbusIP` inside. If you'd have the Ethernet2 shield, for example, one would
use `NetSwarm<ModbusIP2>` instead (and adapt the `#include`s as well).

And in `setup()` and `loop()` we make sure NetSwarm has a part.

Now what does this program do? Not much. It just gets the NetSwarm stack going.
But already you can play with it.


Interacting
-----------

Before we start looking into how different nodes, let's see how we can interact
with one from the PC. There are several options, depending on your platform:

* [netswarm-monitor](https://github.com/wvengen/netswarm-webapp) is a web application
  specifically written for this library, and allows to interact with devices. You do
  need to have [Python](http://python.org/), though.
* When you have installed [Python](http://python.org/), [`pymodbus`](#pymodbus) would be useful (see below).
* On Windows, [ModbusView TCP](https://oceancontrols.com.au/OCS-011.html) might help.


### `pymodbus`

When Python is installed on your computer, [pymodbus](https://github.com/bashwork/pymodbus)
is a convenient way to interact with devices.

First install pymodbus (see its README). Then make sure the Arduino and your
PC are connected to the same network with an ip-address in the range
192.168.1.x. Finally, open a Python shell (e.g. `ipython`).

This example assumes your Arduino is running the
[Dimmer example](examples/Dimmer/Dimmer.pde). Then enter these commands in the
Python shell:

```python
from pymodbus.client.sync import *
c = ModbusTcpClient('192.168.1.177')

# show the current value, which is zero, since the LED is off
c.read_holding_registers(100).registers
# => [0]

# turn the LED on
c.write_register(100, 255);

# show the current value again, which is now 255, the LED is on
c.read_holding_registers(100).registers
# => [255]
```

You see that you can read and write the dimmer register. You can also try other
values between 0 and 255 to dim the LED.

If you want to use UDP instead of TCP, you can create a new client
using `c = ModbusUdpClient('192.168.1.177')`.

**Note** If you have trouble configuring your network, you may open
the file NetSwarm.h and change the default IP-address in the lines with
`#define NETSWARM_IP_START_1` and beyond. Find the IP-address of your
PC (e.g. 10.0.0.100), change the last number (e.g. 10.0.0.80), check
that it isn't used already (run `ping 10.0.0.80` and make sure there is
no response), and update NetSwarm.h. In this case, that would be:

```cpp
#define NETSWARM_IP_START_1 10
#define NETSWARM_IP_START_2  0
#define NETSWARM_IP_START_3  0
#define NETSWARM_IP_START_4 80
```

and upload the sketch again. Now you can connect to that address instead
(e.g. `c = ModbusTcpClient('10.0.0.80')`).


### From an Arduino

You may also want to interact with a NetSwarm device from another Arduino
(which may or may not be running NetSwarm himself). This is part of the
modbus-arduino library, using the `ModbusMasterIP` and `ModbusMasterUDP`
classes. Please see [a network](#a-network) for an example.



A network
---------

Until now, we've just looked at a single device. That's important to get started,
but the real fun starts when there are multiple devices on the network. For that,
you'll need a number of Arduino's with ethernet, a network switch or hub and some
more ethernet cables.

As an example we have a LED connected to each device, and want that all nodes
blink exactly together. In a master-slave setup, you'd just have one master
sending all slaves a command to turn it on or off, but with a swarm of devices
all running exactly the same code, there is a different approach: each device
individually determines if the LED needs to be on or off, and if it sees that
its conclusion is different from the current state, it broadcasts the new value
over the network.

```cpp
#include <SPI.h>
#include <EEPROM.h>
#include <Ethernet.h>
#include <Modbus.h>
#include <ModbusUDP.h>
#include <NetSwarm.h>

const int PIN_LED = 9;
NetSwarm<ModbusUDP> ns;
ModbusMasterUDP mbm;

// our current LED state
bool led = false;
// when was the last state change
unsigned long lastChange = 0;

// define a register number for the dimmer
enum modbusRegister {
  COIL_LED = NETSWARM_MODBUS_OFFSET // we start with register number 100
};

void setup() {
  // setup NetSwarm
  ns.config();
  mbm.config();
  // add a register for the LED
  ns.addCoil(COIL_LED, 0);
  // setup LED output
  pinMode(PIN_LED, OUTPUT);
}

void loop() {
  // handle incoming Modbus messages
  ns.task();
  // change the LED value when it's time
  if ((millis() - lastChange) > (1000 + random(100))) {
    // locally
    bool newLed = !led;
    ns.Coil(COIL_LED, newLed);
    // and on the network
    byte ip[4];
    ns.getIpBcast(ip);
    mbm.sendCoil(ip, COIL_LED, newLed);
    lastChange = millis();
  }
  // finally update our own LED
  if (ns.Coil(COIL_LED) != led) {
    led = ns.Coil(COIL_LED);
    digitalWrite(PIN_LED, led ? HIGH : LOW);
    lastChange = millis();
  }
}
```

The essence here is that each device individually decides whether the LED state
needs to change, and updates the states locally and on the network. To avoid a
packet storm when all devices want to notify the others at the same time, a
small random timing difference is introduced (we could also use the node id here,
but then it will always be the same device responding first).

Note that we need to use UDP here because we're using broadcast.


Development version
-------------------

To install the development version of the NetSwarm library, you can create
the `NetSwarm` folder in your `libraries` folder, and put the files from this
repository in there (most importantly, `NetSwarm.h` and `NetSwarm.tpp`).


License
-------

This library is released under the [MIT license](LICENSE.md).
