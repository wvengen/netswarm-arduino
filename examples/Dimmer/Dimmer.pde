/*
 * NetSwarm Dimmer example
 *
 *
 * Created 22 Aug 2016
 * by wvengen
 * Updated 10 Sep 2016
 * by wvengen
 *
 * https://github.com/wvengen/netswarm-arduino/blob/master/examples/Dimmer/Dimmer.pde
 */
#include <EEPROM.h>
#include <SPI.h>
#include <Ethernet.h>
#include <Modbus.h>
#include <ModbusUDP.h>
#include <NetSwarm.h>

 // the pin that the LED is attached to
const int ledPin = 9;

// define a register number for the dimmer
enum modbusRegister {
  HREG_DIMMER = NETSWARM_MODBUS_OFFSET // we start with register number 100
};

NetSwarm<ModbusUDP> ns;

void setup() {
  // initialize NetSwarm
  ns.config();
  // add a register for the dimmer
  ns.addHreg(HREG_DIMMER);
  // initialize the ledPin as an output
  pinMode(ledPin, OUTPUT);
}

void loop() {
  // handle any pending network requests
  ns.task();
  // update the LED from the Modbus register value
  analogWrite(ledPin, ns.Hreg(HREG_DIMMER));
}

/*
 * To communicate with this over the network, you can use the Python library
 * pymodbus (https://github.com/bashwork/pymodbus).
 *
 * First install that library (see its README). Make sure the Arduino and your
 * PC are connected to the same network with an ip-address in the range
 * 192.168.1.x. Then open Python shell and enter the commands:
 *
 *   from pymodbus.client.sync import *
 *   c = ModbusUdpClient('192.168.1.177')
 *
 *   # show the current value, which is zero, since the LED is off
 *   c.read_holding_registers(100).registers
 *   # => [0]
 *
 *   # turn the LED on
 *   c.write_register(100, 255);
 *
 *   # show the current value again, which is now 255, the LED is on
 *   c.read_holding_registers(100).registers
 *   # => [255]
 *
 * You see that you can read and write the dimmer register. You can also
 * try other values between 0 and 255 to dim the LED.
 *
 * **Note** If you have trouble configuring your network, you may open
 * the file NetSwarm.h and change the default IP-address in the lines with
 * `#define NETSWARM_IP_START_1` and beyond. Find the IP-address of your
 * PC (e.g. 10.0.0.100), change the last number (e.g. 10.0.0.80), check
 * that it isn't used already (run `ping 10.0.0.80` and make sure there is
 * no response), and update NetSwarm.h. In this case, that would be:
 *   #define NETSWARM_IP_START_1 10
 *   #define NETSWARM_IP_START_2  0
 *   #define NETSWARM_IP_START_3  0
 *   #define NETSWARM_IP_START_4 80
 * and upload the sketch again. Now you can connect to that address instead
 * (e.g. `c = ModbusUdpClient('10.0.0.80')`).
 */
