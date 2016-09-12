/*
 * NetSwarm library
 *
 *
 * Note that this implementation uses templates, which means it can't be
 * compiled as a regular C++ file; it is included in the header instead.
 * http://stackoverflow.com/q/495021/2866660
 *
 * Created 05 Aug 2016
 * by wvengen
 * Modified 12 Sep 2016
 * by wvengen
 *
 * https://github.com/wvengen/netswarm-arduino/blob/master/NetSwarm.cpp
 *
 */
#include "NetSwarm.h"

#ifdef USE_NETSWARM_EEPROM
  #include <EEPROM.h>
#endif

#define HREG_IP_ADDR_DEFAULT_1 ((NETSWARM_IP_START_1 << 8) + NETSWARM_IP_START_2)
#define HREG_IP_ADDR_DEFAULT_2 ((NETSWARM_IP_START_3 << 8) + NETSWARM_IP_START_4)

/***
 * Public methods
 */

template<class ModbusT>
NetSwarm<ModbusT>::NetSwarm(byte dataVersion, unsigned int eepromOffset) {
  #ifdef USE_NETSWARM_EEPROM
  this->dataVersion = dataVersion;
  this->eepromOffset = eepromOffset;
  #endif
  this->setupDone = false;
}

template<class ModbusT>
void NetSwarm<ModbusT>::config() {
  #ifdef USE_NETSWARM_EEPROM
  setupEeprom();
  #endif
  setupNetwork();
  setupRegisters();
  setupDone = true;
}

template<class ModbusT>
void NetSwarm<ModbusT>::setCommandCallback(command_callback_t callback) {
  commandCallback = callback;
}

template<class ModbusT>
void NetSwarm<ModbusT>::task() {
  ModbusT::task();
  if (ModbusT::Coil(COIL_APPLY)) {
    setupNetwork();
    ModbusT::Coil(COIL_APPLY, 0);
    if (commandCallback) commandCallback(COIL_APPLY);
  }
  #ifdef USE_NETSWARM_EEPROM
  if (ModbusT::Coil(COIL_SAVE)) {
    saveEeprom();
    ModbusT::Coil(COIL_SAVE, 0);
    if (commandCallback) commandCallback(COIL_SAVE);
  }
  if (ModbusT::Coil(COIL_LOAD)) {
    loadEeprom();
    ModbusT::Coil(COIL_LOAD, 0);
    if (commandCallback) commandCallback(COIL_LOAD);
  }
  #endif
}

#ifdef USE_NETSWARM_EEPROM
template<class ModbusT>
bool NetSwarm<ModbusT>::loadEeprom() {
  // make sure we have valid data in the EEPROM
  setupEeprom();
  if (currentDataVersion == 0 || dataVersion != currentDataVersion) {
    return false;
  }
  // load persisted registers (assumes they have all been setup!)
  // (based on Modbus::searchRegister implementation)
  TPRegister *reg = _persist_regs_head;
  if(reg == 0) return(0);
  word w;
  do {
    w = (EEPROM.read(eepromOffset + 4 + reg->offset * 2 + 0) << 8) +
        (EEPROM.read(eepromOffset + 4 + reg->offset * 2 + 1));
    ModbusT::Hreg(reg->offset, w);
    reg = reg->next;
  } while(reg);
  return true;
}

template<class ModbusT>
void NetSwarm<ModbusT>::saveEeprom() {
  // write header
  EEPROM.write(eepromOffset + 0, (byte)'N');
  EEPROM.write(eepromOffset + 1, (byte)'S');
  EEPROM.write(eepromOffset + 2, (byte)'d');
  EEPROM.write(eepromOffset + 3, dataVersion);
  // store persisted registers (assumes they have all been setup!)
  // (based on Modbus::searchRegister implementation)
  TPRegister *reg = _persist_regs_head;
  if(reg == 0) return;
  word w;
  do {
    w = ModbusT::Hreg(reg->offset);
    EEPROM.write(eepromOffset + 4 + reg->offset * 2 + 0, (byte)(w >> 8));
    EEPROM.write(eepromOffset + 4 + reg->offset * 2 + 1, (byte)(w & 0xff));
    reg = reg->next;
  } while(reg);
}
#endif /* USE_NETSWARM_EEPROM */

template<class ModbusT>
byte NetSwarm<ModbusT>::getId() {
  word v = HregRead(HREG_IP_ADDR_2, HREG_IP_ADDR_DEFAULT_2) & 0xff;
  return v - NETSWARM_IP_START_4;
}

template<class ModbusT>
void NetSwarm<ModbusT>::getIpAddr(byte ip[4]) {
  word value;

  value = HregRead(HREG_IP_ADDR_1, HREG_IP_ADDR_DEFAULT_1);
  ip[0] = value >> 8;
  ip[1] = value & 0xff;
  value = HregRead(HREG_IP_ADDR_2, HREG_IP_ADDR_DEFAULT_2);
  ip[2] = value >> 8;
  ip[3] = value & 0xff;
}

template<class ModbusT>
void NetSwarm<ModbusT>::getIpBcast(byte ip[4]) {
  getIpAddr(ip);
  ip[3] = 0xff; // @todo configurable netmask
}

template<class ModbusT>
void NetSwarm<ModbusT>::getMacAddr(byte mac[6]) {
  mac[0] = 2;          // locally administered mac range, unicast
  getIpAddr(&mac[2]);
}


/***
 * Private methods
 */

// Read Modbus holding register with fallback to EEPROM and given value
template<class ModbusT>
word NetSwarm<ModbusT>::HregRead(word offset, word fallback) {
  // if Modbus was fully setup, use memory registers
  //   enables e.g. applying network settings without saving in EEPROM
  if (setupDone) {
    return ModbusT::Hreg(offset);

  #ifdef USE_NETSWARM_EEPROM
  // if we have valid data in the EEPROM, use that
  // @todo validate EEPROM data version
  } else if (dataVersion == currentDataVersion) {
    return (EEPROM.read(eepromOffset + 4 + offset * 2) << 8) +
           (EEPROM.read(eepromOffset + 4 + offset * 2 + 1));
  #endif /* USE_NETSWARM_EEPROM */

  // otherwise fallback to default given
  } else {
    return fallback;
  }
}

template<class ModbusT>
void NetSwarm<ModbusT>::setupNetwork() {
  byte ip[4];
  byte mac[6];

  getIpAddr(ip);
  getMacAddr(mac);

  ModbusT::config(mac, ip);
}

// Setup default modbus registers
template<class ModbusT>
void NetSwarm<ModbusT>::setupRegisters() {
  // the ip-addresses are expected to be persistant and at the start
  addHregPersist(HREG_IP_ADDR_1, HregRead(HREG_IP_ADDR_1, HREG_IP_ADDR_DEFAULT_1));
  addHregPersist(HREG_IP_ADDR_2, HregRead(HREG_IP_ADDR_2, HREG_IP_ADDR_DEFAULT_2));

  ModbusT::addCoil(COIL_APPLY);
  #ifdef USE_NETSWARM_EEPROM
  ModbusT::addCoil(COIL_SAVE);
  ModbusT::addCoil(COIL_LOAD);
  #endif
}

#ifdef USE_NETSWARM_EEPROM
template<class ModbusT>
void NetSwarm<ModbusT>::setupEeprom() {
  // first make sure we have the magic 'NSd' at the beginning
  if (EEPROM.read(eepromOffset + 0) == (byte)'N' &&
      EEPROM.read(eepromOffset + 1) == (byte)'S' &&
      EEPROM.read(eepromOffset + 2) == (byte)'d') {
    // fourth byte is data version
    currentDataVersion = EEPROM.read(eepromOffset + 3);
  } else {
    currentDataVersion = 0;
  }
}

template<class ModbusT>
void NetSwarm<ModbusT>::setPersist(word offset) {
  // (based on Modbus::addReg implementation)
  TPRegister *reg;
  reg = (TPRegister*) malloc(sizeof(TPRegister));
  reg->offset = offset;
  reg->next = 0;
  if (_persist_regs_head == 0) {
    _persist_regs_head = reg;
    _persist_regs_last = reg;
  } else {
    _persist_regs_last->next = reg;
    _persist_regs_last = reg;
  }
}
#endif /* USE_NETSWARM_EEPROM */
