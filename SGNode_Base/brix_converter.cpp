#include "brix_converter.h"

float BrixConverter::brixToSG(float brix) {
  if (brix < 0.0f) brix = 0.0f;
  return 1.0f + brix / (258.6f - ((brix / 258.2f) * 227.1f));
}
