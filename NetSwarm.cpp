/*
 * NetSwarm library
 *
 *
 * Created 05 Aug 2016
 * by wvengen
 * Modified 22 Aug 2016
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

NetSwarm::NetSwarm(byte dataVersion, unsigned int eepromOffset) {
  #ifdef USE_NETSWARM_EEPROM
  this->dataVersion = dataVersion;
  this->eepromOffset = eepromOffset;
  #endif
  this->setupDone = false;
  this->mb = new NETSWARM_MODBUS_CLASS();
}

void NetSwarm::config() {
  #ifdef USE_NETSWARM_EEPROM
  setupEeprom();
  #endif
  setupNetwork();
  setupRegisters();
  setupDone = true;
}

void NetSwarm::setCommandCallback(command_callback_t callback) {
  commandCallback = callback;
}

void NetSwarm::task() {
  mb->task();
  if (Coil(COIL_APPLY)) {
    setupNetwork();
    Coil(COIL_APPLY, 0);
    if (commandCallback) commandCallback(COIL_APPLY);
  }
  #ifdef USE_NETSWARM_EEPROM
  if (Coil(COIL_SAVE)) {
    saveEeprom();
    Coil(COIL_SAVE, 0);
    if (commandCallback) commandCallback(COIL_SAVE);
  }
  if (Coil(COIL_LOAD)) {
    loadEeprom();
    Coil(COIL_LOAD, 0);
    if (commandCallback) commandCallback(COIL_LOAD);
  }
  #endif
}

#ifdef USE_NETSWARM_EEPROM
bool NetSwarm::loadEeprom() {
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
    mb->Hreg(reg->offset, w);
    reg = reg->next;
  } while(reg);
  return true;
}

void NetSwarm::saveEeprom() {
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
    w = mb->Hreg(reg->offset);
    EEPROM.write(eepromOffset + 4 + reg->offset * 2 + 0, (byte)(w >> 8));
    EEPROM.write(eepromOffset + 4 + reg->offset * 2 + 1, (byte)(w & 0xff));
    reg = reg->next;
  } while(reg);
}
#endif /* USE_NETSWARM_EEPROM */

byte NetSwarm::getId() {
  word v = HregRead(HREG_IP_ADDR_2, HREG_IP_ADDR_DEFAULT_2) & 0xff;
  return v - NETSWARM_IP_START_4;
}

void NetSwarm::getIpAddr(byte ip[4]) {
  word value;

  value = HregRead(HREG_IP_ADDR_1, HREG_IP_ADDR_DEFAULT_1);
  ip[0] = value >> 8;
  ip[1] = value & 0xff;
  value = HregRead(HREG_IP_ADDR_2, HREG_IP_ADDR_DEFAULT_2);
  ip[2] = value >> 8;
  ip[3] = value & 0xff;
}

void NetSwarm::getIpBcast(byte ip[4]) {
  getIpAddr(ip);
  ip[3] = 0xff; // @todo configurable netmask
}

void NetSwarm::getMacAddr(byte mac[6]) {
  mac[0] = 2;          // locally administered mac range, unicast
  getIpAddr(&mac[2]);
}

#ifdef USE_NETSWARM_MASTER
void NetSwarm::sendHreg(IPAddress ip, word offset, word value) {
    // use separate buffers to avoid trouble when packet in coming in
    byte sendbuffer[7 + 5]; // MBAP + frame
    byte *MBAP = &sendbuffer[0];
    byte *frame = &sendbuffer[7];
    word len = 5;

    memset(sendbuffer, 0, sizeof(sendbuffer));

    // setup Modbus frame
    frame[0] = MB_FC_WRITE_REG;
    frame[1] = offset >> 8;
    frame[2] = offset & 0xff;
    frame[3] = value >> 8;
    frame[4] = value & 0xff;

    // setup MBAP header
    MBAP[0] = _masterTransactionId >> 8;
    MBAP[1] = _masterTransactionId & 0xff;
    MBAP[4] = (len+1) >> 8;     //_len+1 for last byte from MBAP
    MBAP[5] = (len+1) & 0x00FF;
    _masterTransactionId++;

    #ifdef USE_NETSWARM_MODBUS_IP
    if (_masterTcp.connect(ip, MODBUSIP_PORT)) {
        _masterTcp.write(sendbuffer, len + 7);
        #ifndef TCP_KEEP_ALIVE
        _masterTcp.stop();
        #endif
    }
    #endif /* USE_NETSWARM_MODBUS_IP */

    #ifdef USE_NETSWARM_MODBUS_UDP
    // setup new udp instance to use other port than slave
    _masterUdp.beginPacket(ip, MODBUSIP_PORT);
    _masterUdp.write(sendbuffer, len + 7);
    _masterUdp.endPacket();
    #endif /* USE_NETSWARM_MODBUS_UDP */
}
#endif /* USE_NETSWARM_MASTER */


/***
 * Private methods
 */

// Read Modbus holding register with fallback to EEPROM and given value
word NetSwarm::HregRead(word offset, word fallback) {
  // if Modbus was fully setup, use memory registers
  //   enables e.g. applying network settings without saving in EEPROM
  if (setupDone) {
    return Hreg(offset);

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

void NetSwarm::setupNetwork() {
  byte ip[4];
  byte mac[6];

  getIpAddr(ip);
  getMacAddr(mac);

  mb->config(mac, ip);
  #ifdef USE_NETSWARM_MASTER
  #ifdef USE_NETSWARM_MODBUS_IP
  // nothing to do
  #endif
  #ifdef USE_NETSWARM_MODBUS_UDP
  _masterUdp.begin(NETSWARM_MASTER_PORT);
  #endif
  #endif /* USE_NETSWARM_MASTER */
}

// Setup default modbus registers
void NetSwarm::setupRegisters() {
  // the ip-addresses are expected to be persistant and at the start
  addHregPersist(HREG_IP_ADDR_1, HregRead(HREG_IP_ADDR_1, HREG_IP_ADDR_DEFAULT_1));
  addHregPersist(HREG_IP_ADDR_2, HregRead(HREG_IP_ADDR_2, HREG_IP_ADDR_DEFAULT_2));

  addCoil(COIL_APPLY);
  #ifdef USE_NETSWARM_EEPROM
  addCoil(COIL_SAVE);
  addCoil(COIL_LOAD);
  #endif
}

#ifdef USE_NETSWARM_EEPROM
void NetSwarm::setupEeprom() {
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

void NetSwarm::setPersist(word offset) {
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
