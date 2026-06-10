#pragma once

#include <Arduino.h>

class RelayDutyWindow {
 public:
  static constexpr uint16_t WINDOW_SECONDS = 600;

  void recordSecond(bool relayOn);
  float dutyPercent() const;
  uint16_t sampleCount() const;

 private:
  uint8_t samples_[WINDOW_SECONDS] = {};
  uint16_t next_ = 0;
  uint16_t count_ = 0;
  uint16_t onCount_ = 0;
};

class CompressorPattern {
 public:
  static constexpr uint16_t PATTERN_MINUTES = 360;

  void recordMinute(bool relayOn);
  bool complete() const;
  void startReplay();
  bool nextReplayMinute();
  uint16_t replayPosition() const;

 private:
  uint8_t samples_[PATTERN_MINUTES] = {};
  uint16_t next_ = 0;
  uint16_t count_ = 0;
  uint16_t replay_ = 0;
};
