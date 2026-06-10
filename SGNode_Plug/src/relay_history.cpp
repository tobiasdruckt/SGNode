#include "relay_history.h"

void RelayDutyWindow::recordSecond(bool relayOn) {
  if (count_ == WINDOW_SECONDS) {
    onCount_ -= samples_[next_];
  } else {
    ++count_;
  }

  samples_[next_] = relayOn ? 1 : 0;
  onCount_ += samples_[next_];
  next_ = (next_ + 1) % WINDOW_SECONDS;
}

float RelayDutyWindow::dutyPercent() const {
  return count_ == 0 ? 0.0f : 100.0f * onCount_ / count_;
}

uint16_t RelayDutyWindow::sampleCount() const {
  return count_;
}

void CompressorPattern::recordMinute(bool relayOn) {
  samples_[next_] = relayOn ? 1 : 0;
  next_ = (next_ + 1) % PATTERN_MINUTES;
  if (count_ < PATTERN_MINUTES) {
    ++count_;
  }
}

bool CompressorPattern::complete() const {
  return count_ == PATTERN_MINUTES;
}

void CompressorPattern::startReplay() {
  replay_ = complete() ? next_ : 0;
}

bool CompressorPattern::nextReplayMinute() {
  if (!complete()) {
    return false;
  }

  const bool relayOn = samples_[replay_] != 0;
  replay_ = (replay_ + 1) % PATTERN_MINUTES;
  return relayOn;
}

uint16_t CompressorPattern::replayPosition() const {
  return replay_;
}
