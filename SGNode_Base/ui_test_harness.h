#ifndef UI_TEST_HARNESS_H
#define UI_TEST_HARNESS_H

#include <Arduino.h>

#ifndef SGNODE_UI_TEST_HARNESS
#define SGNODE_UI_TEST_HARNESS 0  // Temporary debug-only harness. Keep disabled for production builds.
#endif

#if SGNODE_UI_TEST_HARNESS
void handleUITestHarness();
#else
inline void handleUITestHarness() {}
#endif

#endif
