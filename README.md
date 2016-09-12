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
with one from the pc.

**TODO**



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
    led = !led;
    ns.Coil(COIL_LED, led);
    // and on the network
    byte ip[4];
    ns.getIpBcast(ip);
    mbm.sendCoil(ip, COIL_LED, led);
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
