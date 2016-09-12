/*
 * NetSwarm Swarm Blink example
 *
 * This example is meant to be run on multiple devices simultaneously. Make
 * sure you've given each its own IP-address. Then the LEDs of each device
 * should blink together.
 *
 * Created 12 Sep 2016
 * by wvengen
 * Updated 12 Sep 2016
 * by wvengen
 *
 * https://github.com/wvengen/netswarm-arduino/blob/master/examples/SwarmBlink/SwarmBlink.pde
 */
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
  // add a register for the LED
  ns.addCoil(COIL_LED, 0);
  // setup LED output
  pinMode(PIN_LED, OUTPUT);
}

void loop() {
  // handle incoming Modbus messages
  ns.task();
  // change the LED value when it's time
  if ((millis() - lastChange) < (1000 + random(100))) {
    // locally
    ns.Coil(COIL_LED, !led);
    // and on the network
    byte ip[4];
    ns.getIpBcast(ip);
    mbm.sendCoil(ip, COIL_LED, !led);
    lasChange = millis();
  }
  // finally update our own LED
  if (ns.Coil(COIL_LED) != led) {
    led = ns.Coil(COIL_LED);
    digitalWrite(PIN_LED, led ? HIGH : LOW);
    lastChange = millis();
  }
}
