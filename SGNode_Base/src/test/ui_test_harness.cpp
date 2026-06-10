#include "ui_test_harness.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if SGNODE_UI_TEST_HARNESS

extern const char* uiTestScreenName();
extern bool uiTestBuildDump(char* buffer, size_t bufferSize);
extern bool uiTestTap(int x, int y);
extern bool uiTestPress(int x, int y);
extern bool uiTestRelease();
extern bool uiTestSwipe(int x1, int y1, int x2, int y2, int durationMs);
extern bool uiTestTapLabel(const char* text, int* outX, int* outY);
extern bool uiTestTypeText(const char* text);
extern bool uiTestKey(const char* key);
extern bool uiTestVisibleText(const char* text);
extern bool uiTestValueEquals(const char* fieldName, const char* expected);
extern bool uiTestStateEquals(const char* stateName);
extern void uiTestSetMockSG(float value);
extern void uiTestSetMockTemp(float value);
extern void uiTestSetMockTime(unsigned long value);
extern bool uiTestRunAnalyzer();
extern bool uiTestRunStateMachine();
extern bool uiTestRunETA();
extern bool uiTestRunRecommendations();
extern bool uiTestCreateBatch();
extern bool uiTestDeleteBatch();
extern bool uiTestMarkTestBatchCompleted();
extern bool uiTestSelfTest(char* buffer, size_t bufferSize);
extern bool uiTestRunRegressionTests(char* buffer, size_t bufferSize);
extern bool uiTestRunBatchRestoreTests(char* buffer, size_t bufferSize);
extern bool uiTestRunFallbackLogTests(char* buffer, size_t bufferSize);
extern bool uiTestRunCompletedBatchTests(char* buffer, size_t bufferSize);
extern bool uiTestRunManageBrewUITests(char* buffer, size_t bufferSize);
extern bool uiTestPrepareManageBrewComplete(char* buffer, size_t bufferSize);
extern bool uiTestOpenManageBrewCompleteDialog(char* buffer, size_t bufferSize);
extern bool uiTestConfirmManageBrewComplete(char* buffer, size_t bufferSize);
extern bool uiTestRunInputValidationTests(char* buffer, size_t bufferSize);
extern bool uiTestRunSensorEdgeTests(char* buffer, size_t bufferSize);
extern bool uiTestRunStateLogicTests(char* buffer, size_t bufferSize);
extern bool uiTestRunUISafetyTests(char* buffer, size_t bufferSize);
extern bool uiTestBatteryStats(char* buffer, size_t bufferSize);
extern bool uiTestBatchDiagnostics(char* buffer, size_t bufferSize);
extern bool uiTestFloatStats(char* buffer, size_t bufferSize);
extern bool uiTestActionStatus(char* buffer, size_t bufferSize);
extern bool uiTestActionDone(char* buffer, size_t bufferSize);
extern bool uiTestActionSkip(char* buffer, size_t bufferSize);
extern bool uiTestRunActionButtonTests(char* buffer, size_t bufferSize);
extern void checkTouch();
extern bool screenDirty;
extern bool staticElementsDrawn;
extern bool topbarDirty;

static void forceRedrawFlags() {
  screenDirty = true;
  staticElementsDrawn = false;
  topbarDirty = true;
}

static char commandBuffer[160];
static size_t commandLength = 0;
static unsigned long waitUntilMs = 0;

static void printOK(const char* message) {
  Serial.print("OK");
  if (message && message[0]) {
    Serial.print(' ');
    Serial.print(message);
  }
  Serial.println();
}

static void printERR(const char* message) {
  Serial.print("ERR");
  if (message && message[0]) {
    Serial.print(' ');
    Serial.print(message);
  }
  Serial.println();
}

static char* skipSpaces(char* text) {
  while (*text == ' ' || *text == '\t') text++;
  return text;
}

static bool parseQuoted(char* text, char* out, size_t outSize) {
  text = skipSpaces(text);
  if (*text != '"') return false;
  text++;
  size_t len = 0;
  while (*text && *text != '"' && len + 1 < outSize) {
    out[len++] = *text++;
  }
  out[len] = '\0';
  return *text == '"';
}

static void handleCommand(char* line) {
  char* args = strchr(line, ' ');
  if (args) {
    *args++ = '\0';
    args = skipSpaces(args);
  } else {
    args = line + strlen(line);
  }

  if (strcmp(line, "help") == 0) {
    #if SGNODE_UI_TEST_MINIMAL
      printOK("commands=help,build_info,screen,set_mock_sg,set_mock_temp,set_mock_time,run_analyzer,run_state_machine,run_eta,run_recommendations,action_status,action_done,action_skip,batch_diagnostics,float_stats,battery_stats,force_redraw");
    #else
      printOK("commands=help,build_info,screen,dump_ui,tap,press,release,swipe,tap_label,type,key,wait,force_redraw,expect_screen,expect_text,expect_not_text,expect_value,expect_state,set_mock_sg,set_mock_temp,set_mock_time,run_analyzer,run_state_machine,run_eta,run_recommendations,create_test_batch,delete_test_batch,mark_test_batch_completed,battery_stats,batch_diagnostics,float_stats,action_status,action_done,action_skip,run_action_button_tests,selftest,prepare_manage_brew_complete,open_manage_brew_complete_dialog,confirm_manage_brew_complete,run_input_validation_tests,run_sensor_edge_tests,run_state_logic_tests,run_ui_safety_tests,run_batch_restore_tests,run_fallback_log_tests,run_completed_batch_tests,run_manage_brew_ui_tests,run_ui_regression_tests");
    #endif
    return;
  }

  if (strcmp(line, "build_info") == 0) {
    #if SGNODE_UI_TEST_MINIMAL
      printOK("build=alpha_0_5_0_ui_harness_minimal");
    #else
      printOK("build=alpha_0_5_0_float_ack_retry_2026_05_23");
    #endif
    return;
  }

  #if SGNODE_UI_TEST_MINIMAL
  if (strcmp(line, "screen") == 0) {
    Serial.print("OK screen ");
    Serial.println(uiTestScreenName());
    return;
  }
  if (strcmp(line, "force_redraw") == 0) {
    forceRedrawFlags();
    printOK("redraw_forced");
    return;
  }
  if (strcmp(line, "set_mock_sg") == 0) {
    uiTestSetMockSG(atof(args));
    printOK("mock_sg_set");
    return;
  }
  if (strcmp(line, "set_mock_temp") == 0) {
    uiTestSetMockTemp(atof(args));
    printOK("mock_temp_set");
    return;
  }
  if (strcmp(line, "set_mock_time") == 0) {
    uiTestSetMockTime(strtoul(args, NULL, 10));
    printOK("mock_time_set");
    return;
  }
  if (strcmp(line, "run_analyzer") == 0) {
    if (uiTestRunAnalyzer()) {
      forceRedrawFlags();
      printOK("analyzer_run");
    } else {
      printERR("analyzer_failed");
    }
    return;
  }
  if (strcmp(line, "run_state_machine") == 0) {
    if (uiTestRunStateMachine()) {
      forceRedrawFlags();
      printOK("state_machine_run");
    } else {
      printERR("state_machine_failed");
    }
    return;
  }
  if (strcmp(line, "run_eta") == 0) {
    if (uiTestRunETA()) {
      forceRedrawFlags();
      printOK("eta_run");
    } else {
      printERR("eta_failed");
    }
    return;
  }
  if (strcmp(line, "run_recommendations") == 0) {
    if (uiTestRunRecommendations()) {
      forceRedrawFlags();
      printOK("recommendations_run");
    } else {
      printERR("recommendations_failed");
    }
    return;
  }
  if (strcmp(line, "battery_stats") == 0) {
    char result[260];
    if (uiTestBatteryStats(result, sizeof(result))) {
      Serial.print("OK ");
      Serial.println(result);
    } else {
      Serial.print("ERR ");
      Serial.println(result);
    }
    return;
  }
  if (strcmp(line, "batch_diagnostics") == 0) {
    char result[260];
    if (uiTestBatchDiagnostics(result, sizeof(result))) {
      Serial.print("OK ");
      Serial.println(result);
    } else {
      Serial.print("ERR ");
      Serial.println(result);
    }
    return;
  }
  if (strcmp(line, "float_stats") == 0) {
    char result[260];
    if (uiTestFloatStats(result, sizeof(result))) {
      Serial.print("OK ");
      Serial.println(result);
    } else {
      Serial.print("ERR ");
      Serial.println(result);
    }
    return;
  }
  if (strcmp(line, "action_status") == 0) {
    char result[180];
    if (uiTestActionStatus(result, sizeof(result))) {
      Serial.print("OK ");
      Serial.println(result);
    } else {
      Serial.print("ERR ");
      Serial.println(result);
    }
    return;
  }
  if (strcmp(line, "action_done") == 0 || strcmp(line, "action_skip") == 0) {
    char result[180];
    bool ok = strcmp(line, "action_done") == 0
      ? uiTestActionDone(result, sizeof(result))
      : uiTestActionSkip(result, sizeof(result));
    if (ok) {
      forceRedrawFlags();
      Serial.print("OK ");
      Serial.println(result);
    } else {
      Serial.print("ERR ");
      Serial.println(result);
    }
    return;
  }
  printERR("unknown_command");
  return;
  #endif

  if (strcmp(line, "screen") == 0) {
    Serial.print("OK screen ");
    Serial.println(uiTestScreenName());
    return;
  }

  if (strcmp(line, "dump_ui") == 0) {
    char dump[900];
    if (uiTestBuildDump(dump, sizeof(dump))) {
      Serial.print("OK ui ");
      Serial.println(dump);
    } else {
      printERR("dump_failed");
    }
    return;
  }

  if (strcmp(line, "tap") == 0) {
    int x, y;
    if (sscanf(args, "%d %d", &x, &y) == 2 && uiTestTap(x, y)) {
      forceRedrawFlags();
      char msg[48];
      snprintf(msg, sizeof(msg), "tapped x=%d y=%d", x, y);
      printOK(msg);
    } else {
      printERR("tap_failed");
    }
    return;
  }

  if (strcmp(line, "press") == 0) {
    int x, y;
    if (sscanf(args, "%d %d", &x, &y) == 2 && uiTestPress(x, y)) {
      forceRedrawFlags();
      char msg[48];
      snprintf(msg, sizeof(msg), "pressed x=%d y=%d", x, y);
      printOK(msg);
    } else {
      printERR("press_failed");
    }
    return;
  }

  if (strcmp(line, "release") == 0) {
    if (uiTestRelease()) {
      forceRedrawFlags();
      printOK("released");
    }
    else printERR("release_failed");
    return;
  }

  if (strcmp(line, "swipe") == 0) {
    int x1, y1, x2, y2, durationMs;
    if (sscanf(args, "%d %d %d %d %d", &x1, &y1, &x2, &y2, &durationMs) == 5 &&
        uiTestSwipe(x1, y1, x2, y2, durationMs)) {
      forceRedrawFlags();
      printOK("swiped");
    } else {
      printERR("swipe_failed");
    }
    return;
  }

  if (strcmp(line, "tap_label") == 0) {
    char text[48];
    int x = 0, y = 0;
    if (parseQuoted(args, text, sizeof(text)) && uiTestTapLabel(text, &x, &y)) {
      forceRedrawFlags();
      char msg[96];
      snprintf(msg, sizeof(msg), "tapped label=\"%s\" x=%d y=%d", text, x, y);
      printOK(msg);
    } else {
      printERR("label_not_found");
    }
    return;
  }

  if (strcmp(line, "type") == 0) {
    char text[80];
    if (parseQuoted(args, text, sizeof(text)) && uiTestTypeText(text)) {
      forceRedrawFlags();
      char msg[100];
      snprintf(msg, sizeof(msg), "typed \"%s\"", text);
      printOK(msg);
    } else {
      printERR("type_failed");
    }
    return;
  }

  if (strcmp(line, "key") == 0) {
    if (uiTestKey(args)) {
      forceRedrawFlags();
      char msg[64];
      snprintf(msg, sizeof(msg), "key %s", args);
      printOK(msg);
    } else {
      printERR("key_failed");
    }
    return;
  }

  if (strcmp(line, "wait") == 0) {
    int ms = atoi(args);
    if (ms < 0) ms = 0;
    waitUntilMs = millis() + (unsigned long)ms;
    char msg[48];
    snprintf(msg, sizeof(msg), "wait %d", ms);
    printOK(msg);
    return;
  }

  if (strcmp(line, "force_redraw") == 0) {
    forceRedrawFlags();
    printOK("redraw_forced");
    return;
  }

  if (strcmp(line, "expect_screen") == 0) {
    if (strcmp(args, uiTestScreenName()) == 0) {
      char msg[64];
      snprintf(msg, sizeof(msg), "screen %s", args);
      printOK(msg);
    } else {
      char msg[96];
      snprintf(msg, sizeof(msg), "expected_screen %s actual=%s", args, uiTestScreenName());
      printERR(msg);
    }
    return;
  }

  if (strcmp(line, "expect_text") == 0 || strcmp(line, "expect_not_text") == 0) {
    char text[64];
    bool wantFound = strcmp(line, "expect_text") == 0;
    bool found = parseQuoted(args, text, sizeof(text)) && uiTestVisibleText(text);
    if (found == wantFound) {
      char msg[96];
      snprintf(msg, sizeof(msg), "%s \"%s\"", found ? "text_found" : "text_absent", text);
      printOK(msg);
    } else {
      char msg[96];
      snprintf(msg, sizeof(msg), "%s \"%s\"", found ? "unexpected_text" : "missing_text", text);
      printERR(msg);
    }
    return;
  }

  if (strcmp(line, "expect_value") == 0) {
    char field[32];
    char expected[64];
    if (sscanf(args, "%31s", field) == 1) {
      char* quote = strchr(args, '"');
      if (quote && parseQuoted(quote, expected, sizeof(expected)) && uiTestValueEquals(field, expected)) {
        printOK("value_match");
      } else {
        printERR("value_mismatch");
      }
    } else {
      printERR("bad_expect_value");
    }
    return;
  }

  if (strcmp(line, "expect_state") == 0) {
    if (uiTestStateEquals(args)) printOK("state_match");
    else printERR("state_mismatch");
    return;
  }

  if (strcmp(line, "set_mock_sg") == 0) {
    uiTestSetMockSG(atof(args));
    printOK("mock_sg_set");
    return;
  }

  if (strcmp(line, "set_mock_temp") == 0) {
    uiTestSetMockTemp(atof(args));
    printOK("mock_temp_set");
    return;
  }

  if (strcmp(line, "set_mock_time") == 0) {
    uiTestSetMockTime(strtoul(args, NULL, 10));
    printOK("mock_time_set");
    return;
  }

  if (strcmp(line, "run_analyzer") == 0) {
    if (uiTestRunAnalyzer()) {
      forceRedrawFlags();
      printOK("analyzer_run");
    }
    else printERR("analyzer_failed");
    return;
  }

  if (strcmp(line, "run_state_machine") == 0) {
    if (uiTestRunStateMachine()) {
      forceRedrawFlags();
      printOK("state_machine_run");
    }
    else printERR("state_machine_failed");
    return;
  }

  if (strcmp(line, "run_eta") == 0) {
    if (uiTestRunETA()) {
      forceRedrawFlags();
      printOK("eta_run");
    }
    else printERR("eta_failed");
    return;
  }

  if (strcmp(line, "run_recommendations") == 0) {
    if (uiTestRunRecommendations()) {
      forceRedrawFlags();
      printOK("recommendations_run");
    }
    else printERR("recommendations_failed");
    return;
  }

  if (strcmp(line, "create_test_batch") == 0) {
    if (uiTestCreateBatch()) {
      forceRedrawFlags();
      printOK("test_batch_created");
    }
    else printERR("create_test_batch_failed");
    return;
  }

  if (strcmp(line, "delete_test_batch") == 0) {
    if (uiTestDeleteBatch()) {
      forceRedrawFlags();
      printOK("test_batch_deleted");
    }
    else printERR("delete_test_batch_failed");
    return;
  }

  if (strcmp(line, "mark_test_batch_completed") == 0) {
    if (uiTestMarkTestBatchCompleted()) {
      forceRedrawFlags();
      printOK("test_batch_completed");
    }
    else printERR("mark_completed_failed");
    return;
  }

  if (strcmp(line, "battery_stats") == 0) {
    char result[260];
    if (uiTestBatteryStats(result, sizeof(result))) {
      Serial.print("OK ");
      Serial.println(result);
    } else {
      Serial.print("ERR ");
      Serial.println(result);
    }
    return;
  }

  if (strcmp(line, "batch_diagnostics") == 0) {
    char result[260];
    if (uiTestBatchDiagnostics(result, sizeof(result))) {
      Serial.print("OK ");
      Serial.println(result);
    } else {
      Serial.print("ERR ");
      Serial.println(result);
    }
    return;
  }

  if (strcmp(line, "float_stats") == 0) {
    char result[260];
    if (uiTestFloatStats(result, sizeof(result))) {
      Serial.print("OK ");
      Serial.println(result);
    } else {
      Serial.print("ERR ");
      Serial.println(result);
    }
    return;
  }

  if (strcmp(line, "action_status") == 0) {
    char result[180];
    if (uiTestActionStatus(result, sizeof(result))) {
      Serial.print("OK ");
      Serial.println(result);
    } else {
      Serial.print("ERR ");
      Serial.println(result);
    }
    return;
  }

  if (strcmp(line, "action_done") == 0 || strcmp(line, "action_skip") == 0) {
    char result[180];
    bool ok = strcmp(line, "action_done") == 0
      ? uiTestActionDone(result, sizeof(result))
      : uiTestActionSkip(result, sizeof(result));
    if (ok) {
      Serial.print("OK ");
      Serial.println(result);
    } else {
      Serial.print("ERR ");
      Serial.println(result);
    }
    return;
  }

  if (strcmp(line, "run_action_button_tests") == 0) {
    char result[180];
    if (uiTestRunActionButtonTests(result, sizeof(result))) {
      Serial.print("OK ");
      Serial.println(result);
    } else {
      Serial.print("ERR ");
      Serial.println(result);
    }
    return;
  }

  if (strcmp(line, "selftest") == 0) {
    char result[180];
    if (uiTestSelfTest(result, sizeof(result))) {
      Serial.print("OK ");
      Serial.println(result);
    } else {
      Serial.print("ERR ");
      Serial.println(result);
    }
    return;
  }

  if (strcmp(line, "run_ui_regression_tests") == 0) {
    char result[180];
    if (uiTestRunRegressionTests(result, sizeof(result))) {
      Serial.print("OK ");
      Serial.println(result);
    } else {
      Serial.print("ERR ");
      Serial.println(result);
    }
    return;
  }

  if (strcmp(line, "run_batch_restore_tests") == 0) {
    char result[180];
    if (uiTestRunBatchRestoreTests(result, sizeof(result))) {
      Serial.print("OK ");
      Serial.println(result);
    } else {
      Serial.print("ERR ");
      Serial.println(result);
    }
    return;
  }

  if (strcmp(line, "run_fallback_log_tests") == 0) {
    char result[180];
    if (uiTestRunFallbackLogTests(result, sizeof(result))) {
      Serial.print("OK ");
      Serial.println(result);
    } else {
      Serial.print("ERR ");
      Serial.println(result);
    }
    return;
  }

  if (strcmp(line, "run_completed_batch_tests") == 0) {
    char result[180];
    if (uiTestRunCompletedBatchTests(result, sizeof(result))) {
      Serial.print("OK ");
      Serial.println(result);
    } else {
      Serial.print("ERR ");
      Serial.println(result);
    }
    return;
  }

  if (strcmp(line, "run_manage_brew_ui_tests") == 0) {
    char result[180];
    if (uiTestRunManageBrewUITests(result, sizeof(result))) {
      Serial.print("OK ");
      Serial.println(result);
    } else {
      Serial.print("ERR ");
      Serial.println(result);
    }
    return;
  }

  if (strcmp(line, "prepare_manage_brew_complete") == 0) {
    char result[180];
    if (uiTestPrepareManageBrewComplete(result, sizeof(result))) {
      Serial.print("OK ");
      Serial.println(result);
    } else {
      Serial.print("ERR ");
      Serial.println(result);
    }
    return;
  }

  if (strcmp(line, "open_manage_brew_complete_dialog") == 0) {
    char result[180];
    if (uiTestOpenManageBrewCompleteDialog(result, sizeof(result))) {
      Serial.print("OK ");
      Serial.println(result);
    } else {
      Serial.print("ERR ");
      Serial.println(result);
    }
    return;
  }

  if (strcmp(line, "confirm_manage_brew_complete") == 0) {
    char result[180];
    if (uiTestConfirmManageBrewComplete(result, sizeof(result))) {
      Serial.print("OK ");
      Serial.println(result);
    } else {
      Serial.print("ERR ");
      Serial.println(result);
    }
    return;
  }

  if (strcmp(line, "run_input_validation_tests") == 0) {
    char result[180];
    if (uiTestRunInputValidationTests(result, sizeof(result))) {
      Serial.print("OK ");
      Serial.println(result);
    } else {
      Serial.print("ERR ");
      Serial.println(result);
    }
    return;
  }

  if (strcmp(line, "run_sensor_edge_tests") == 0) {
    char result[180];
    if (uiTestRunSensorEdgeTests(result, sizeof(result))) {
      Serial.print("OK ");
      Serial.println(result);
    } else {
      Serial.print("ERR ");
      Serial.println(result);
    }
    return;
  }

  if (strcmp(line, "run_state_logic_tests") == 0) {
    char result[180];
    if (uiTestRunStateLogicTests(result, sizeof(result))) {
      Serial.print("OK ");
      Serial.println(result);
    } else {
      Serial.print("ERR ");
      Serial.println(result);
    }
    return;
  }

  if (strcmp(line, "run_ui_safety_tests") == 0) {
    char result[180];
    if (uiTestRunUISafetyTests(result, sizeof(result))) {
      Serial.print("OK ");
      Serial.println(result);
    } else {
      Serial.print("ERR ");
      Serial.println(result);
    }
    return;
  }

  printERR("unknown_command");
}

void handleUITestHarness() {
  if (waitUntilMs && millis() < waitUntilMs) {
    checkTouch();
    return;
  }
  waitUntilMs = 0;

  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      commandBuffer[commandLength] = '\0';
      if (commandLength > 0) handleCommand(commandBuffer);
      commandLength = 0;
    } else if (commandLength + 1 < sizeof(commandBuffer)) {
      commandBuffer[commandLength++] = c;
    }
  }
}

#endif
