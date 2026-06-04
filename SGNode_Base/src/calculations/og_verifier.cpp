#include "og_verifier.h"

const float OGVerifier::THRESHOLD = 0.006f;

OGVerifier::OGVerifier() {
  reset();
}

void OGVerifier::reset() {
  readings[0] = readings[1] = readings[2] = 0.0f;
  count = 0;
}

OGVerificationResult OGVerifier::addReading(float sg) {
  readings[count % 3] = sg;
  count++;

  OGVerificationResult result;
  result.ready = false;
  result.needsUserChoice = false;
  result.measuredOG = 0.0f;
  result.ogDifference = 0.0f;

  if (count < 3) return result;

  float a = readings[(count - 1) % 3];
  float b = readings[(count - 2) % 3];
  float c = readings[(count - 3) % 3];
  float maxSG = a;
  if (b > maxSG) maxSG = b;
  if (c > maxSG) maxSG = c;
  float minSG = a;
  if (b < minSG) minSG = b;
  if (c < minSG) minSG = c;

  if ((maxSG - minSG) <= 0.003f || count >= 10) {
    result.ready = true;
    result.measuredOG = (a + b + c) / 3.0f;
  }

  return result;
}

bool OGVerifier::exceedsThreshold(float difference) {
  if (difference < 0.0f) difference = -difference;
  return difference > THRESHOLD;
}
