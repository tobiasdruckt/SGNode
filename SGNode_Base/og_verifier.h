#ifndef OG_VERIFIER_H
#define OG_VERIFIER_H

struct OGVerificationResult {
  bool ready;
  bool needsUserChoice;
  float measuredOG;
  float ogDifference;
};

class OGVerifier {
public:
  OGVerifier();
  void reset();
  OGVerificationResult addReading(float sg);
  static bool exceedsThreshold(float difference);
  static const float THRESHOLD;

private:
  float readings[3];
  int count;
};

#endif
