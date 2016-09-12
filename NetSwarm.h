/*
 * NetSwarm library
 *
 *
 * Created 05 Aug 2016
 * by wvengen
 * Modified 22 Aug 2016
 * by wvengen
 *
 * https://github.com/wvengen/netswarm-arduino/blob/master/NetSwarm.h
 *
 */
#ifndef __NETSWARM_H__
#define __NETSWARM_H__

// feature toggles
#ifndef DONT_USE_NETSWARM_EEPROM
  #define USE_NETSWARM_EEPROM 1
#endif

// ip range starts at 192.168.1.177
#ifndef NETSWARM_IP_START_1
  #define NETSWARM_IP_START_1 192
#endif
#ifndef NETSWARM_IP_START_2
  #define NETSWARM_IP_START_2 168
#endif
#ifndef NETSWARM_IP_START_3
  #define NETSWARM_IP_START_3   1
#endif
#ifndef NETSWARM_IP_START_4
  #define NETSWARM_IP_START_4 177
#endif

enum netswarmModbusRegister {
  // ip address (default 192.168.1.177) *
  HREG_IP_ADDR_1, // first two bytes
  HREG_IP_ADDR_2, // last two bytes

  // commands: write 1 to execute
  //   EEPROM-related commands are also defined when not enabled
  //   to keep compatibility of register offsets (will be a no-op)
  COIL_APPLY, // apply changes (for registers marked with *)
  COIL_SAVE,  // store registers in EEPROM
  COIL_LOAD,  // (re)load registers from EEPROM

  // use this as the first user register
  NETSWARM_MODBUS_OFFSET = 100
};

// private API, don't use (except in NetSwarm)
typedef struct TPRegister {
  word offset;
  struct TPRegister *next;
} TPRegister;

// command callback prototype
typedef void (*command_callback_t)(enum netswarmModbusRegister);

template<class ModbusT>
class NetSwarm : public ModbusT {
public:
  NetSwarm(byte dataVersion = 1, unsigned int eepromOffset = 0);
  void config();
  void task();
  void setCommandCallback(command_callback_t callback);

  #ifdef USE_NETSWARM_EEPROM
  bool loadEeprom();
  void saveEeprom();
  #endif

  // return swarm identifier (based on IP address)
  byte getId();
  // return current IP address
  void getIpAddr(byte ip[4]);
  // return current broadcast address
  void getIpBcast(byte ip[4]);
  // return current MAC address (based on IP address)
  void getMacAddr(byte mac[6]);

  inline void addHregPersist(word offset, word value = 0) {
    ModbusT::addHreg(offset, value);
    #ifdef USE_NETSWARM_EEPROM
    setPersist(offset);
    #endif
  }
 
private:
  // whether setup is done
  bool setupDone;
  // optional callback to run on COIL_APPLY
  command_callback_t commandCallback;

  #ifdef USE_NETSWARM_EEPROM
  // offset in the EEPROM where data is stored (in case you have other things there)
  unsigned int eepromOffset;
  // EEPROM data version that the program expects
  byte dataVersion;
  // current EEPROM data version (used during initialization) - 0 means invalid
  byte currentDataVersion;

  // registers that are persisted in EEPROM
  TPRegister *_persist_regs_head;
  TPRegister *_persist_regs_last;
  #endif

  void setupNetwork();
  void setupRegisters();

  word HregRead(word offset, word fallback);

  #ifdef USE_NETSWARM_EEPROM
  void setupEeprom();
  void setPersist(word addr);
  #endif
};

// need to include implementation when using C++ templates
#include "NetSwarm.tpp"

#endif /* __NETSWARM_H__ */
