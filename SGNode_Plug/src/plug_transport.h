#pragma once

#include <Arduino.h>
#include <sg_protocol.h>

class PlugTransport {
 public:
  bool begin();
  bool hasCommand() const;
  sg_plug_command_t command() const;
  uint32_t lastCommandMs() const;
  bool sendStatus(sg_plug_status_t* status);

 private:
  static void onReceive(uint8_t* mac, uint8_t* data, uint8_t len);
  static PlugTransport* instance_;

  sg_plug_command_t command_ = {};
  bool hasCommand_ = false;
  uint32_t lastCommandMs_ = 0;
  uint16_t statusSequence_ = 0;
};
