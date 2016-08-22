/*
 * NetSwarm IP Address example
 *
 *
 * Created 22 Aug 2016
 * by wvengen
 *
 * https://github.com/wvengen/netswarm-arduino/blob/master/examples/IPAddress/IPAddress.pde
 */
#include <EEPROM.h>
#include <SPI.h>
#include <Ethernet.h>
#include <Modbus.h>
#include <ModbusUDP.h>
#include <NetSwarm.h>

NetSwarm ns;

// declare functions we're using so we can reference them
void showIpAddr();
void commandCallback(enum netswarmModbusRegister cmd);

void setup() {
  // initialize NetSwarm
  ns.config();
  // initialize serial port
  Serial.begin(9600);
  // show IP address whenever changes are applied
  ns.setCommandCallback(commandCallback);
  // show the IP address right now too
  showIpAddr();
}

void loop() {
  // handle any pending network requests
  ns.task();
}

void showIpAddr() {
  // get the current IP address
  byte ip[4];
  ns.getIpAddr(ip);

  // and print it on the serial console
  Serial.print("IP address: ");
  Serial.print(ip[0]);
  Serial.print(".");
  Serial.print(ip[1]);
  Serial.print(".");
  Serial.print(ip[2]);
  Serial.print(".");
  Serial.print(ip[3]);
  Serial.println();
}

void commandCallback(enum netswarmModbusRegister cmd) {
  if (cmd == COIL_APPLY) {
    showIpAddr();
  }
}

/*
 * To communicate with this over the network, you can use the Python library
 * pymodbus (https://github.com/bashwork/pymodbus). For more information on
 * that, please see the Dimmer example.
 *
 *   # create a new Modbus UDP client
 *   from pymodbus.client.sync import *
 *   c = ModbusUdpClient('192.168.1.177')
 *   # define some registers so it reads nicer
 *   HREG_IP_ADDR_1 = 0
 *   COIL_APPLY     = 3
 *
 *   # show the current IP address
 *   registers = c.read_holding_registers(HREG_IP_ADDR_1, 2).registers
 *   reduce(lambda r,x: r + [x >> 8, x & 0xff], registers, [])
 *   # => [192, 168, 1, 177]
 *
 *   # now we're going to move this node to address 192.168.1.178
 *   c.write_registers(HREG_IP_ADDR_1, [(192 << 8) + 168, (1 << 8) + 178])
 *   # we still need to apply the change
 *   c.write_coil(COIL_APPLY, 1)
 *
 *   # now we need to connect to the new address
 *   c = ModbusUdpClient('192.168.1.178')
 *   # again show the current IP address
 *   registers = c.read_holding_registers(HREG_IP_ADDR_1, 2).registers
 *   reduce(lambda r,x: r + [x >> 8, x & 0xff], registers, [])
 *   # => [192, 168, 1, 178]
 *
 *   # when we reboot the node, it will revert to 192.168.1.177,
 *   # but we can write the changes to EEPROM so it will remember.
 *   c.write_coil(COIL_SAVE, 1)
 *
 *   # now reset the Arduino node, and find it out still works
 *   registers = c.read_holding_registers(HREG_IP_ADDR_1, 2).registers
 *   reduce(lambda r,x: r + [x >> 8, x & 0xff], registers, [])
 *   # => [192, 168, 1, 178]
 *
 * If you open the serial monitor during this exercise, you should be able
 * to see when IP address changes there as well.
 *
 */
