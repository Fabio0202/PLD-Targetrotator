#ifndef READ_SERIAL_COMMAND_H
#define READ_SERIAL_COMMAND_H

#include <Arduino.h>

namespace readSerialCommand 
{
  void setup();
  void update();
  void processCommand(const String& command);
}

#endif