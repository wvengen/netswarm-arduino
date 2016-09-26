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
#include <ModbusIP.h>
#include <NetSwarm.h>

 // the pin that the LED is attached to
const int ledPin = 9;

// define a register number for the dimmer
enum modbusRegister {
  HREG_DIMMER = NETSWARM_MODBUS_OFFSET // we start with register number 100
};

NetSwarm<ModbusIP> ns;

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
