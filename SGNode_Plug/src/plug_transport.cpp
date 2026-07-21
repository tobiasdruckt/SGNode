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

bool validFloat(float value, float minimum, float maximum) {
  return isfinite(value) && value >= minimum && value <= maximum;
}

bool validSeconds(uint16_t value, uint16_t minimum, uint16_t maximum) {
  return value >= minimum && value <= maximum;
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

bool PlugTransport::sendStatus(sg_plug_status_v2_t* status) {
  if (!status) {
    return false;
  }
  status->packet_type = SG_PLUG_STATUS_TYPE;
  status->version = SG_PROTOCOL_VERSION;
  status->sequence_id = ++statusSequence_;
  status->crc = sg_crc16(reinterpret_cast<const uint8_t*>(status),
                         offsetof(sg_plug_status_v2_t, crc));
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
  if (!validFloat(incoming.beer_target_c, -5.0f, 35.0f) ||
      !validFloat(incoming.ramp_k_per_h, 0.0f, 2.0f) ||
      !validFloat(incoming.batch_liters, 1.0f, 100.0f) ||
      !validFloat(incoming.controller_kp, 0.05f, 5.0f) ||
      !validFloat(incoming.controller_tn_h, 0.0f, 72.0f) ||
      !validFloat(incoming.controller_d_brake_h, 0.0f, 6.0f) ||
      !validFloat(incoming.air_turn_off_above_target_c, -1.0f, 3.0f) ||
      !validFloat(incoming.air_turn_on_above_target_c, 0.0f, 5.0f) ||
      !validSeconds(incoming.air_minimum_on_s, 0, 1800) ||
      !validSeconds(incoming.air_minimum_off_s, 0, 3600) ||
      !validFloat(incoming.cold_integral_band_c, 0.0f, 5.0f) ||
      !validFloat(incoming.warm_integral_band_c, 0.0f, 5.0f) ||
      !validFloat(incoming.max_positive_integral_c, 0.0f, 5.0f) ||
      !validFloat(incoming.max_negative_integral_c, -5.0f, 0.0f) ||
      !validFloat(incoming.integral_leak_per_hour, 0.0f, 5.0f) ||
      !validFloat(incoming.error_crossing_keep_factor, 0.0f, 1.0f) ||
      !validFloat(incoming.max_d_offset_c, 0.0f, 5.0f) ||
      !validFloat(incoming.warming_d_factor, 0.0f, 1.0f) ||
      !validFloat(incoming.beer_undershoot_lockout_c, 0.0f, 2.0f) ||
      !validFloat(incoming.fast_warming_rate_k_per_h, 0.0f, 5.0f) ||
      !validFloat(incoming.strong_undershoot_c, 0.0f, 5.0f) ||
      !validFloat(incoming.strong_undershoot_air_offset_c, 0.0f, 5.0f) ||
      !validFloat(incoming.min_air_target_c, -5.0f, 20.0f) ||
      !validFloat(incoming.max_air_target_c, 0.0f, 35.0f) ||
      incoming.max_air_target_c <= incoming.min_air_target_c ||
      !validFloat(incoming.target_step_c, 0.0f, 5.0f) ||
      !validFloat(incoming.ramp_controller_kp_h, 0.0f, 6.0f) ||
      !validFloat(incoming.ramp_controller_tn_h, 0.0f, 24.0f) ||
      !validFloat(incoming.max_ramp_trim_c, 0.0f, 3.0f) ||
      !validFloat(incoming.ramp_fade_distance_c, 0.0f, 5.0f) ||
      !validFloat(incoming.rate_filter_samples, 1.0f, 20.0f) ||
      incoming.air_turn_on_above_target_c < incoming.air_turn_off_above_target_c) {
    return;
  }

  instance_->command_ = incoming;
  instance_->hasCommand_ = true;
  instance_->lastCommandMs_ = millis();
}
