#include "plug_transport.h"

#include <ESP8266WiFi.h>
extern "C" {
#include <espnow.h>
#include <user_interface.h>
}

namespace {

constexpr uint8_t ESPNOW_CHANNEL = 1;
uint8_t baseMac[6] = {0xA4, 0xF0, 0x0F, 0x68, 0x22, 0x00};

bool macMatches(const uint8_t* left, const uint8_t* right) {
  return memcmp(left, right, 6) == 0;
}

}  // namespace

PlugTransport* PlugTransport::instance_ = nullptr;

bool PlugTransport::begin() {
  instance_ = this;
  WiFi.mode(WIFI_STA);
  wifi_set_channel(ESPNOW_CHANNEL);
  if (esp_now_init() != 0) {
    return false;
  }
  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_register_recv_cb(onReceive);
  if (esp_now_is_peer_exist(baseMac)) {
    return true;
  }
  return esp_now_add_peer(baseMac, ESP_NOW_ROLE_COMBO, ESPNOW_CHANNEL, nullptr, 0) == 0;
}

bool PlugTransport::hasCommand() const {
  return hasCommand_;
}

sg_plug_command_t PlugTransport::command() const {
  return command_;
}

uint32_t PlugTransport::lastCommandMs() const {
  return lastCommandMs_;
}

bool PlugTransport::sendStatus(sg_plug_status_t* status) {
  if (!status) {
    return false;
  }
  status->packet_type = SG_PLUG_STATUS_TYPE;
  status->version = SG_PROTOCOL_VERSION;
  status->sequence_id = ++statusSequence_;
  status->crc = sg_crc16(reinterpret_cast<const uint8_t*>(status),
                         offsetof(sg_plug_status_t, crc));
  return esp_now_send(baseMac, reinterpret_cast<uint8_t*>(status), sizeof(*status)) == 0;
}

void PlugTransport::onReceive(uint8_t* mac, uint8_t* data, uint8_t len) {
  if (!instance_ || !mac || !data || !macMatches(mac, baseMac) ||
      len != sizeof(sg_plug_command_t)) {
    return;
  }

  sg_plug_command_t incoming;
  memcpy(&incoming, data, sizeof(incoming));
  const uint16_t crc = sg_crc16(reinterpret_cast<const uint8_t*>(&incoming),
                                offsetof(sg_plug_command_t, crc));
  if (incoming.packet_type != SG_PLUG_COMMAND_TYPE ||
      incoming.version != SG_PROTOCOL_VERSION || incoming.crc != crc) {
    return;
  }
  if (!isfinite(incoming.beer_target_c) || incoming.beer_target_c < -5.0f ||
      incoming.beer_target_c > 35.0f ||
      !isfinite(incoming.ramp_k_per_h) || incoming.ramp_k_per_h < 0.0f ||
      incoming.ramp_k_per_h > 2.0f ||
      !isfinite(incoming.batch_liters) || incoming.batch_liters < 1.0f ||
      incoming.batch_liters > 100.0f) {
    return;
  }

  instance_->command_ = incoming;
  instance_->hasCommand_ = true;
  instance_->lastCommandMs_ = millis();
}
