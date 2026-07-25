# SGNode_Base/SGNode_Base.ino

**Type:** Arduino Sketch
**Hash:** `13bce0a2e95b785e0de0d517c402f774edc6f345c7a4d577b6123963380441d2`

## Overview / Role

Core functionality module

## Verified API & Symbols

### Structs & Classes

- `struct Theme`
  - `uint16_t background`
  - `uint16_t cardBackground`
  - `uint16_t buttonInactive`
  - `uint16_t primary`
  - `uint16_t primaryText`
  - `uint16_t accent`
  - `uint16_t accentText`
  - `uint16_t gold`
  - `uint16_t textPrimary`
  - `uint16_t textSecondary`
  - `uint16_t textMuted`
  - `uint16_t success`
  - `uint16_t warning`
  - `uint16_t error`
  - `uint16_t info`
  - `uint16_t border`
  - `uint16_t gridLine`
  - `uint16_t graphPurple`
  - `uint16_t graphBlue`
  - `uint16_t graphGreen`

- `struct BufferedData`
  - `payload_t data`
  - `uint32_t epoch_s`
  - `unsigned long buffered_millis`
  - `bool written`

- `struct GraphPoint`
  - `int x`
  - `int y`
  - `bool valid`

- `struct PlugGovernorSettings`
  - `float kp`
  - `float integralTnHours`
  - `float dBrakeHours`
  - `float airTurnOffAboveTargetC`
  - `float airTurnOnAboveTargetC`
  - `uint16_t airMinimumOnS`
  - `uint16_t airMinimumOffS`
  - `float coldIntegralBandC`
  - `float warmIntegralBandC`
  - `float maxPositiveIntegralC`
  - `float maxNegativeIntegralC`
  - `float integralLeakPerHour`
  - `float errorCrossingKeepFactor`
  - `float maxDOffsetC`
  - `float warmingDFactor`
  - `float beerUndershootLockoutC`
  - `float fastWarmingRateKPerH`
  - `float strongUndershootC`
  - `float strongUndershootAirOffsetC`
  - `float minAirTargetC`
  - `float maxAirTargetC`
  - `float targetStepC`
  - `float rampControllerKpHours`
  - `float rampControllerTnHours`
  - `float maxRampTrimC`
  - `float rampFadeDistanceC`
  - `float rateFilterSamples`

- `struct PlugTestPhase`
  - `float targetC`
  - `uint32_t holdSeconds`
  - `float minBeerC`
  - `bool hasMinBeerC`

- `struct UITestObject`
  - `const char* id`
  - `const char* type`
  - `const char* label`
  - `int x`
  - `int y`
  - `int w`
  - `int h`
  - `bool enabled`
  - `bool visible`

- `struct EditableYeastPreset`
  - `char id[24]`
  - `char displayName[32]`
  - `char category[32]`
  - `int typicalAttenuationMin`
  - `int typicalAttenuationMax`
  - `int defaultAttenuation`
  - `char fermentationSpeed[16]`
  - `float lagPhaseHours`
  - `float typicalDurationHours`
  - `float recommendedTempMinC`
  - `float recommendedTempMaxC`
  - `char flocculation[16]`
  - `char curveTemplate[32]`
  - `bool diacetylRestRecommended`
  - `char notes[64]`

- `struct ManagedBatchSummary`
  - `bool loaded`
  - `bool completed`
  - `bool postPackageTracking`
  - `char style[24]`
  - `float liters`
  - `int points`
  - `uint32_t firstEpoch`
  - `uint32_t lastEpoch`
  - `char status[18]`
  - `char evaluation[32]`

### Functions

- `void setup (void)`

- `void loop (void)`

- `void bcdToDec (uint8_t value)`

- `void decToBcd (uint8_t value)`

- `void daysFromCivil (int year, unsigned month, unsigned day)`

- `void dateTimeToEpoch (uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second)`

- `void civilFromDays (int64_t z, uint16_t* year, uint8_t* month, uint8_t* day)`

- `void epochToDateTime (uint32_t epoch, uint16_t* year, uint8_t* month, uint8_t* day, uint8_t* hour, uint8_t* minute, uint8_t* second)`

- `void isEpochValid (uint32_t epoch)`

- `void compileMonthNumber (const char* monthName) const`

- `void buildEpochWithUploadOffset (void)`

- `void readRTCDateTime (uint16_t* year, uint8_t* month, uint8_t* day, uint8_t* hour, uint8_t* minute, uint8_t* second)`

- `void setRTCDateTime (uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second)`

- `void getCurrentEpoch (void)`

- `void formatTimestamp (uint32_t epoch, char* buf, size_t bufSize)`

- `void parseDateTimeToEpoch (const char* timestamp) const`

- `void syncSystemTimeFromRTC (uint32_t epoch)`

- `void initRTC (void)`

- `void initESPNow (void)`

- `void esp_now_register_recv_cb ([](const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) const`

- `void setFloatZeroCalStatus (const char* message) const`

- `void sendDataAck (const uint8_t* mac, const payload_t& data) const`

- `void isDuplicateFloatSequence (uint16_t sequence_id)`

- `void rememberFloatSequence (uint16_t sequence_id)`

- `void initDisplay (void)`

- `void drawCurrentScreen (void)`

- `void initTouch (void)`

- `void ensureFloatPeerRegistered (const uint8_t* mac) const`

- `void onPlugStatusReceived (const uint8_t *mac, const uint8_t *incomingData, int len) const`

- `void hasValidPlugBeerTemperature (void)`

- `void currentPlugBeerTemperatureC (void)`

- `void hasValidPlugAirTemperature (void)`

- `void hasFreshPlugStatus (void)`

- `void usesPlugTemperatureAsPrimary (void)`

- `void isPostPackageTemperaturePhase (void)`

- `void isFloatInactiveAfterPackaging (void)`

- `void shouldShowFloatStillActiveWarning (void)`

- `void effectiveTemperatureC (float floatTemperatureC)`

- `void latestDisplayedTemperatureC (void)`

- `void sendPlugCommandIfDue (void)`

- `void onDataReceived (const uint8_t *mac, const uint8_t *incomingData, int len) const`

- `void addDataPoint (payload_t data, uint32_t epoch_s)`

- `void clearHistoricalDisplayData (void)`

- `void clearBufferedSensorData (void)`

- `void shouldRecordBatchFloatData (void)`

- `void finalizeHistoricalDataLoad (void)`

- `void markScreenDirtyForFloatData (void)`

- `void displayBufferIndexForPoint (int pointIndex)`

- `void timestampForPoint (int pointIndex)`

- `void timestampsAvailableForGraph (void)`

- `void graphXForPoint (int pointIndex, int graphX, int graphW)`

- `void calculateTimestampTrend (float latestDensity, uint32_t latestEpoch, float* trendOut)`

- `void drawLiveViewStatic (void)`

- `void updateLiveViewDynamic (void)`

- `void drawFloatOffAngleTile (int x, int y, int w, int h)`

- `void drawLiveView (void)`

- `void drawGraphView (void)`

- `void drawRecommendationPanel (int x, int y, int w, int h)`

- `void formatActionHeader (char* buffer, size_t bufferSize)`

- `void drawDashboardScreen (void)`

- `void copyPresetToDraft (const YeastPreset* preset) const`

- `void optionIndex (const char* value, const char* const* options, int count) const`

- `void selectOptionOffset (char* target, size_t targetSize, const char* const* options, int count, int offset) const`

- `void beginNewYeastWizard (const char* presetId) const`

- `void loadYeastStepBuffer (void)`

- `void commitYeastStepBuffer (void)`

- `void drawNewYeastNav (const char* nextLabel) const`

- `void drawYeastDropdown (const char* label, const char* value) const`

- `void drawNewYeastScreen (void)`

- `void saveNewYeastWizard (void)`

- `void handleNewYeastTouch (int x, int y)`

- `void loadManagedYeasts (void)`

- `void drawManageYeastScreen (void)`

- `void handleManageYeastTouch (int x, int y)`

- `void batchNumberFromId (const char* id) const`

- `void loadManagedBatches (void)`

- `void nextBatchNumberOnSD (void)`

- `void continueManagedBatch (const char* batchId) const`

- `void copyManagedBatch (const char* batchId) const`

- `void buildManagedBatchChildPath (const char* parentPath, const char* entryName, char* path, size_t pathSize) const`

- `void deleteManagedBatchPath (const char* path) const`

- `void deleteManagedBatch (const char* batchId) const`

- `void markManagedBatchCompleted (const char* batchId) const`

- `void loadManagedBatchSummary (const char* batchId, ManagedBatchSummary* summary) const`

- `void drawManageBrewScreen (void)`

- `void handleManageBrewTouch (int x, int y)`

- `void drawBrewWizardScreen (void)`

- `void drawOGVerificationScreen (void)`

- `void targetChartHours (void)`

- `void temperatureChartHours (void)`

- `void postPackageTemperatureStartEpoch (void)`

- `void postPackageTemperatureStartHour (void)`

- `void postPackageTemperatureChartHours (void)`

- `void targetStartOffsetHours (void)`

- `void fermentationElapsedSecondsAt (unsigned long epoch)`

- `void targetModelHourForChartHour (float chartHour)`

- `void targetChartHourForModelHour (float modelHour)`

- `void expectedTargetSGAtChartHour (float chartHour)`

- `void resetPlugTestScheduleState (void)`

- `void parseJsonNumberAfter (const char* start, const char* key, float fallback) const`

- `void boundedOrDefault (float value, float fallback, float minimum, float maximum)`

- `void boundedSecondsOrDefault (float value, uint16_t fallback, uint16_t minimum, uint16_t maximum)`

- `void normalizePlugGovernorSettings (void)`

- `void resetPlugGovernorSettingsToDefaults (void)`

- `void savePlugGovernorSettingsToSD (void)`

- `void ensurePlugGovernorSettingsLoaded (void)`

- `void plugGovernorSettingLabel (uint8_t index)`

- `void plugGovernorSettingValue (uint8_t index)`

- `void setPlugGovernorSettingValue (uint8_t index, float value)`

- `void plugGovernorSettingStep (uint8_t index)`

- `void formatPlugGovernorSettingValue (uint8_t index, char* buffer, size_t bufferSize)`

- `void adjustPlugGovernorSetting (int direction)`

- `void loadPlugTestScheduleIfNeeded (void)`

- `void plugTestScheduleTargetC (float fallbackTarget, unsigned long nowEpoch)`

- `void legacyTemperatureTargetC (unsigned long nowEpoch)`

- `void currentTemperaturePhase (unsigned long nowEpoch)`

- `void currentTemperatureTargetC (unsigned long nowEpoch)`

- `void currentTemperatureRampKPerHour (unsigned long nowEpoch)`

- `void currentDisplayPhaseName (unsigned long nowEpoch)`

- `void targetHourForSG (float targetSG, float chartHours)`

- `void targetHourForAttenuation (float attenuationPercent, float chartHours)`

- `void eventHourFromEpoch (unsigned long eventEpoch)`

- `void graphXForHour (float hour, int graphX, int graphW, float chartHours)`

- `void rampedProfileTemperatureAtHour (float hour, TemperatureProfileRuntimePhase* phaseOut)`

- `void pointHourForBatchChart (int pointIndex)`

- `void drawTargetEventMarker (int gx, int gy, int gw, int gh, float chartHours, float hour, const char* label, uint16_t color, int labelSlot, int labelSide) const`

- `void drawTargetChartSpanLabel (int gx, int gy, int gw, float chartHours, float startHour, float endHour, const char* label, uint16_t color, int row) const`

- `void drawTargetChartXAxis (int gx, int gy, int gw, int gh, float chartHours)`

- `void drawTargetChartEventMarkers (int gx, int gy, int gw, int gh, float chartHours, bool includePostPackage)`

- `void drawPostPackageTemperatureEventMarkers (int gx, int gy, int gw, int gh, float chartHours)`

- `void drawTargetVsActualChart (void)`

- `void drawGrid (int graphX, int graphY, int graphW, int graphH, float minDensity, float maxDensity)`

- `void drawGraphLine (int x0, int y0, int x1, int y1, uint16_t color)`

- `void angleForTargetSG (float targetSG)`

- `void drawGraph (void)`

- `void drawGraphForMetric (GraphMetric metric)`

- `void drawCalibrationView (void)`

- `void drawBatteryView (void)`

- `void drawTempGraphView (void)`

- `void drawTempGraphModeToggle (int contentY)`

- `void drawAngleGraphView (void)`

- `void drawAbvGraphView (void)`

- `void drawButton (int x, int y, int w, int h, const char* label, bool active) const`

- `void drawThemeToggle (int x, int y, bool enabled)`

- `void loadThemePreference (void)`

- `void saveThemePreference (void)`

- `void toggleTheme (void)`

- `void isLogoBackgroundPixel (uint16_t color)`

- `void loadBacklightPreference (void)`

- `void saveBacklightPreference (void)`

- `void applyBacklightLevel (void)`

- `void initBacklight (void)`

- `void cycleBacklightLevel (void)`

- `void baseOtaDiagHtml (void)`

- `void baseOtaPageHtml (void)`

- `void syncTime (void)`

- `void then (function(r)`

- `void then (function(t)`

- `void startBaseOtaMode (void)`

- `void on ("/", HTTP_GET, []()`

- `void on ("/diag", HTTP_GET, []()`

- `void on ("/logo.png", HTTP_GET, []()`

- `void on ("/reboot", HTTP_GET, []()`

- `void on ("/settime", HTTP_GET, []()`

- `void on ("/update", HTTP_POST, []()`

- `void stopBaseOtaAndReboot (void)`

- `void handleBaseOtaMode (void)`

- `void drawCenteredDialogFrame (int boxX, int boxY, int boxW, int boxH)`

- `void drawBaseOtaScreen (void)`

- `void drawBaseOtaConfirmDialog (void)`

- `void drawFloatOtaPopup (void)`

- `void drawTopbarPulldown (void)`

- `void handleOverlayTouch (int x, int y)`

- `void onFloatOtaStatusReceived (const uint8_t* mac, const uint8_t* data, int len) const`

- `void drawBootScreen (void)`

- `void checkTouch (void)`

- `void sendCalibrationCommand (uint8_t command, float target_sg)`

- `void applyCalibration (void)`

- `void sendExitCalibrationCommand (void)`

- `void abortCalibrationFlow (void)`

- `void onCalibrationResponse (const uint8_t *mac, const uint8_t *incomingData, int len) const`

- `void onCalibrationCoefficients (const uint8_t *mac, const uint8_t *incomingData, int len) const`

- `void onCalibrationCommandFromFloat (const uint8_t *mac, const uint8_t *incomingData, int len) const`

- `void copyCsvField (const char* line, int fieldIndex, char* out, size_t outSize) const`

- `void parseCsvFloatField (const char* line, int fieldIndex, float* valueOut) const`

- `void isPlausibleCsvBeerTemperature (float value)`

- `void isFermentationCsvDataPoint (const payload_t& data) const`

- `void isPostPackageCsvTemperaturePoint (const payload_t& data, uint32_t epoch_s) const`

- `void parsePostPackageCsvTemperaturePoint (const char* line, payload_t* data, uint32_t* epoch_s, uint8_t* battery_percent, float* temperatureOut) const`

- `void clearPostPackageTemperatureGraphData (void)`

- `void appendPostPackageTemperatureGraphData (uint32_t epoch_s, float temperatureC)`

- `void loadPostPackageTemperatureGraphData (void)`

- `void parseCSVDataLine (const char* line, payload_t* data, uint32_t* epoch_s, uint8_t* battery_percent) const`

- `void loadHistoricalDataFromCSV (const char* filename, GraphLoadMode mode, int maxPoints, int samplingStep) const`

- `void formatDurationShort (unsigned long seconds, char* buffer, size_t bufferSize)`

- `void startNewBatchStorage (const char* batchId) const`

- `void ensureFermentationLogFile (const char* batchId) const`

- `void createFreshBatchLogFile (const char* batchId) const`

- `void rollbackBatchFiles (const char* batchId) const`

- `void findBestFallbackBatch (char* batchId, size_t bufferSize)`

- `void persistActiveBrewProfile (void)`

- `void beginBrewWizard (void)`

- `void cancelBrewWizard (void)`

- `void completeBrewWizard (void)`

- `void handleOGChoice (bool useMeasuredOG)`

- `void handleCurrentActionChoice (bool done)`

- `void saveYeastPerformanceSummary (float finalGravity, uint32_t completedAt)`

- `void isPlausibleSensorReading (const payload_t& data, const char** issue) const`

- `void updateFermentationAssistant (payload_t data, uint32_t epoch_s)`

- `void refreshCurrentBatchAction (float currentSG, unsigned long nowEpoch)`

- `void evaluateBatchActionWithHysteresis (FermentationPhase phase, float attenuation, float currentSG, float gravityDeltaPerHour, unsigned long nowEpoch)`

- `void autoStartColdCrashIfPlugReady (unsigned long nowEpoch)`

- `void shouldKeepDryHopActionLatched (const BatchAction& previousAction, const BatchAction& nextAction, float currentSG, float gravityDeltaPerHour) const`

- `void makeLatchedDryHopAction (float currentSG)`

- `void smoothedGravityDeltaPerHour (int maxReadings)`

- `void refreshFermentationAssistantFromProfile (void)`

- `void migrateActiveBrewProfileFromHistory (const char* logPath) const`

- `void checkExistingFermentation (void)`

- `void createNewFermentationFile (void)`

- `void logDataToSD (payload_t data, uint32_t epoch_s)`

- `void logPostPackagingTemperatureIfDue (void)`

- `void drawCreateNewDialog (void)`

- `void logOGToSD (void)`

- `void checkOGStability (float currentSG)`

- `void calculateABV (float og, float currentSG)`

- `void calculateBatteryPercentage (float voltage)`

- `void getLatestDisplayBatteryVoltage (float* voltageOut)`

- `void drawViewTopbar (const char* title) const`

- `void shouldShowSdWarning (void)`

- `void drawSdWarningTopbar (void)`

- `void loadAllHistoryForDetailChart (void)`

- `void drawNoDataCard (const char* message, const char* subtext, int activeTab) const`

- `void drawDetailSection (int* y, const char* title) const`

- `void drawDetailRow (int* y, const char* label, const char* value) const`

- `void drawDetailSectionAt (int x, int* y, int w, const char* title) const`

- `void drawDetailRowAt (int x, int* y, int w, const char* label, const char* value) const`

- `void drawWrappedText (int x, int y, int w, const char* text, uint16_t color) const`

- `void updateChargingState (float currentVoltage)`

- `void calculateDischargeRateLinearRegression (void)`

- `void drawMoreView (void)`

- `void drawLiveDetailsView (void)`

- `void drawPlugGovernorView (void)`

- `void handleLiveDetailsTouch (int x, int y)`

- `void handlePlugGovernorTouch (int x, int y)`

- `void mountSDTemporarily (void)`

- `void dismountSD (void)`

- `void bufferSensorData (payload_t data, uint32_t epoch_s)`

- `void writeBufferedDataToSD (void)`

- `void isSafeForSDOperation (void)`

- `void loadFloatMacFromEEPROM (void)`

- `void saveFloatMacToEEPROM (void)`

- `void loadPlugMacFromEEPROM (void)`

- `void savePlugMacToEEPROM (void)`

- `void startWait (int durationMs)`

- `void drawPolynomialGraph (void)`

- `void calculateGraphPoints (void)`

- `void drawGraphAxes (void)`

- `void drawGraphCurve (void)`

- `void calibrationPointCount (void)`

- `void calibrationPointIndex (CalibMode mode)`

- `void calibrationModeForPoint (int index)`

- `void calibrationTargetSG (int index)`

- `void calibrationAddAmount (int index)`

- `void checkWaitTimeout (void)`

- `void stopWait (void)`

- `void formatTimeFromDataPoint (int index, char* buf, int bufSize)`

- `void updateCursorPosition (void)`

- `void drawCursorValueDisplay (int x, int y, bool showRight)`

- `void handleGraphButton6h (void)`

- `void handleGraphButton24h (void)`

- `void handleGraphButtonAll (void)`

- `void handleGraphButtonLeft (void)`

- `void handleGraphButtonRight (void)`

- `void uiTestWizardStepName (BrewWizardStep step)`

- `void uiTestScreenName (void)`

- `void uiTestObjects (UITestObject* objects, int maxObjects)`

- `void uiTestBuildDump (char* buffer, size_t bufferSize)`

- `void uiTestElementMatches (const UITestObject& object, const char* text) const`

- `void uiTestTap (int x, int y)`

- `void uiTestPress (int x, int y)`

- `void uiTestRelease (void)`

- `void uiTestSwipe (int x1, int y1, int x2, int y2, int durationMs)`

- `void uiTestTapLabel (const char* text, int* outX, int* outY) const`

- `void uiTestTapTextKey (char c)`

- `void uiTestTapNumberKey (char c)`

- `void uiTestTypeText (const char* text) const`

- `void uiTestKey (const char* key) const`

- `void uiTestVisibleText (const char* text) const`

- `void uiTestValueEquals (const char* fieldName, const char* expected) const`

- `void uiTestStateEquals (const char* stateName) const`

- `void uiTestSetMockSG (float value)`

- `void uiTestSetMockTemp (float value)`

- `void uiTestSetMockTime (unsigned long value)`

- `void uiTestRunAnalyzer (void)`

- `void uiTestRunStateMachine (void)`

- `void uiTestRunETA (void)`

- `void uiTestRunRecommendations (void)`

- `void uiTestRemoveFileIfExists (const char* path) const`

- `void uiTestDeleteBatchById (const char* batchId) const`

- `void uiTestCreateBatch (void)`

- `void uiTestDeleteBatch (void)`

- `void uiTestMarkTestBatchCompleted (void)`

- `void uiTestBatteryStats (char* buffer, size_t bufferSize)`

- `void uiTestBatchDiagnostics (char* buffer, size_t bufferSize)`

- `void uiTestFloatStats (char* buffer, size_t bufferSize)`

- `void uiTestActionStatus (char* buffer, size_t bufferSize)`

- `void uiTestActionDone (char* buffer, size_t bufferSize)`

- `void uiTestActionSkip (char* buffer, size_t bufferSize)`

- `void uiTestRunActionButtonTests (char* buffer, size_t bufferSize)`

- `void uiTestSelfTest (char* buffer, size_t bufferSize)`

- `void uiTestFileExists (const char* path) const`

- `void uiTestTestBatchFilesExist (bool* profileOK, bool* targetOK, bool* logOK)`

- `void uiTestReadLastLogEpoch (const char* path, uint32_t* epochOut) const`

- `void uiTestWriteMixedEpochLog (const char* path, uint32_t createdAt) const`

- `void uiTestWriteDelayedFermentationLog (const char* path, uint32_t createdAt) const`

- `void uiTestCreateNumberedBatch (int number, const char* name, uint32_t createdAt) const`

- `void uiTestFindHighestBatchWithProfile (void)`

- `void uiTestDeleteNumberedBatch (int number)`

- `void uiTestRunBatchRestoreTests (char* buffer, size_t bufferSize)`

- `void uiTestRunFallbackLogTests (char* buffer, size_t bufferSize)`

- `void uiTestRunCompletedBatchTests (char* buffer, size_t bufferSize)`

- `void uiTestRunManageBrewUITests (char* buffer, size_t bufferSize)`

- `void uiTestPrepareManageBrewComplete (char* buffer, size_t bufferSize)`

- `void uiTestOpenManageBrewCompleteDialog (char* buffer, size_t bufferSize)`

- `void uiTestConfirmManageBrewComplete (char* buffer, size_t bufferSize)`

- `void uiTestPrintResult (bool ok, const char* name, const char* step, const char* expected, const char* actual) const`

- `void uiTestRunOne (const char* name, bool condition, const char* step, const char* expected, const char* actual, int* total, int* passed, int* failed, char* firstFail, size_t firstFailSize) const`

- `void uiTestRegressionValidFloat (float value)`

- `void uiTestRunInputValidationTests (char* buffer, size_t bufferSize)`

- `void uiTestRunSensorEdgeTests (char* buffer, size_t bufferSize)`

- `void uiTestRunStateLogicTests (char* buffer, size_t bufferSize)`

- `void uiTestRunUISafetyTests (char* buffer, size_t bufferSize)`

- `void uiTestRunRegressionTests (char* buffer, size_t bufferSize)`

- `void batteryDebugPrint (bool ok, const char* message) const`

- `void batteryDebugStats (char* buffer, size_t bufferSize)`

- `void batteryDebugBatchDiagnostics (char* buffer, size_t bufferSize)`

- `void batteryDebugFloatStats (char* buffer, size_t bufferSize)`

- `void batteryDebugDumpCsv (void)`

- `void batteryDebugHandleCommand (char* command)`

- `void handleBatteryDebugSerial (void)`

### Enums & Constants

- enum DebugLevel
  - `DEBUG_QUIET`
  - `DEBUG_ERROR`
  - `DEBUG_INFO`
  - `DEBUG_VERBOSE`

- enum GraphLoadMode
  - `LOAD_6H`
  - `LOAD_24H`
  - `LOAD_ALL`

- enum ChargingState
  - `CHARGING_UNKNOWN`
  - `CHARGING`
  - `DISCHARGING`

- enum ViewMode
  - `LIVE_VIEW`
  - `GRAPH_VIEW`
  - `CALIBRATION_VIEW`
  - `BATTERY_VIEW`
  - `MORE_VIEW`
  - `LIVE_DETAILS_VIEW`
  - `POLY_GRAPH_VIEW`
  - `TEMP_GRAPH_VIEW`
  - `ANGLE_GRAPH_VIEW`
  - `ABV_GRAPH_VIEW`
  - `BREW_WIZARD_VIEW`
  - `OG_VERIFICATION_VIEW`
  - `TARGET_CHART_VIEW`
  - `DASHBOARD_VIEW`
  - `NEW_YEAST_VIEW`
  - `MANAGE_YEAST_VIEW`
  - `MANAGE_BREW_VIEW`
  - `PLUG_GOVERNOR_VIEW`

- enum GraphMetric
  - `METRIC_DENSITY`
  - `METRIC_TEMPERATURE`
  - `METRIC_ANGLE`
  - `METRIC_ABV`

- enum TempGraphMode
  - `TEMP_GRAPH_FERMENT`
  - `TEMP_GRAPH_POST_PACKAGE`

- enum CalibMode
  - `CALIB_IDLE`
  - `CALIB_SETUP`
  - `CALIB_INSTRUCTIONS`
  - `CALIB_OFFSET`
  - `CALIB_POINT1`
  - `CALIB_POINT2`
  - `CALIB_POINT3`
  - `CALIB_POINT4`
  - `CALIB_POINT5`
  - `CALIB_POINT6`
  - `CALIB_COMPLETE`
  - `CALIB_APPLYING`
  - `CALIB_FAILED`
  - `CALIB_SKIP_WARNING`

- enum NewYeastStep
  - `NY_NAME`
  - `NY_CATEGORY`
  - `NY_ATT_MIN`
  - `NY_ATT_MAX`
  - `NY_ATT_DEFAULT`
  - `NY_SPEED`
  - `NY_LAG`
  - `NY_DURATION`
  - `NY_TEMP_MIN`
  - `NY_TEMP_MAX`
  - `NY_FLOC`
  - `NY_CURVE`
  - `NY_DREST`
  - `NY_NOTES`
  - `NY_REVIEW`

### Macros

- `#define SGNODE_UI_TEST_HARNESS 0  // Debug-only serial UI harness. Keep disabled for production builds.`

- `#define SGNODE_UI_TEST_MINIMAL 0  // 1 = minimal harness commands (no UI injection path)`

- `#define SGNODE_BATTERY_DEBUG 0  // Tiny serial diagnostics for battery/history checks. Disabled in production.`

- `#define ACK_PACKET_TYPE 0xA5`

- `#define SCREEN_W 480  // Landscape width (320x480 native rotated)`

- `#define SCREEN_H 320  // Landscape height`

- `#define DISPLAY_CS 15   // LCD Chip Select`

- `#define DISPLAY_DC 2    // Data/Command`

- `#define DISPLAY_RST -1   // Reset (uses ESP32 reset)`

- `#define TFT_GRAY 0x8410  // Dark gray color`

- `#define SGNODE_DEBUG_LEVEL DEBUG_ERROR`

- `#define LOG_ERROR(...) do { if (SGNODE_DEBUG_LEVEL >= DEBUG_ERROR) Serial.printf(__VA_ARGS__); } while (0)`

- `#define LOG_ERRORLN(msg) do { if (SGNODE_DEBUG_LEVEL >= DEBUG_ERROR) Serial.println(msg); } while (0)`

- `#define LOG_INFO(...) do { if (SGNODE_DEBUG_LEVEL >= DEBUG_INFO) Serial.printf(__VA_ARGS__); } while (0)`

- `#define LOG_INFOLN(msg) do { if (SGNODE_DEBUG_LEVEL >= DEBUG_INFO) Serial.println(msg); } while (0)`

- `#define LOG_VERBOSE(...) do { if (SGNODE_DEBUG_LEVEL >= DEBUG_VERBOSE) Serial.printf(__VA_ARGS__); } while (0)`

- `#define LOG_VERBOSELN(msg) do { if (SGNODE_DEBUG_LEVEL >= DEBUG_VERBOSE) Serial.println(msg); } while (0)`

- `#define SD_CS 5   // SD card chip select`

- `#define SD_MOSI 23   // SD card SPI bus write data signal`

- `#define SD_MISO 19   // SD card SPI bus read data signal`

- `#define SD_SCK 18   // SD card SPI bus clock signal`

- `#define SD_ENABLED true`

- `#define RTC_SDA_PIN 32`

- `#define RTC_SCL_PIN 25`

- `#define RTC_I2C_ADDR 0x68`

- `#define RTC_COMPILE_UPLOAD_OFFSET_S 11`

- `#define RTC_VALID_FROM_YEAR 2024`

- `#define ESPNOW_CHANNEL 1`

- `#define MAX_DATA_BUFFER 500  // Buffer up to 500 sensor readings (increased from 100)`

- `#define MAX_DATA_POINTS 150`

- `#define CSV_LINE_BUFFER_SIZE 384`

- `#define FERMENTATION_LOG_HEADER "timestamp,epoch_s,uptime_s,angle,density,temperature,battery_voltage,battery_percent,state,current_attenuation,estimated_abv,recommendation_code,float_temperature,plug_beer_temperature,plug_air_temperature,plug_beer_target,plug_air_target,plug_mode,plug_relay,plug_duty_10m,plug_faults,plug_pi_offset,plug_pi_tn,plug_beer_rate,plug_kp,plug_d_brake,plug_p_offset,plug_i_offset,plug_d_offset,plug_ramp_trim,plug_ramp_i,plug_ramp_rate_error,plug_beer_rate_raw"`

- `#define POST_PACKAGE_TEMP_LOG_INTERVAL_MS(10UL * 60UL * 1000UL) // Separate buffer for discharge rate calculation (25 recent points only)`

- `#define OG_STABILITY_THRESHOLD 0.005 // SG variance threshold for stability`

- `#define OG_MAX_READINGS 10     // Force capture at this reading if not stable`

- `#define EEPROM_BYTES 160`

- `#define EEPROM_FLOAT_MAC_MAGIC 100`

- `#define EEPROM_FLOAT_MAC_ADDR 104`

- `#define EEPROM_THEME_MAGIC 120`

- `#define EEPROM_THEME_VALUE 124`

- `#define EEPROM_PLUG_MAC_MAGIC 132`

- `#define EEPROM_PLUG_MAC_ADDR 136`

- `#define EEPROM_BRIGHTNESS_MAGIC 148`

- `#define EEPROM_BRIGHTNESS_VALUE 152`

- `#define FLOAT_MAC_MAGIC 0x4D414346 // "MACF" in hex`

- `#define THEME_MAGIC 0x54484D45 // "THME" in hex`

- `#define PLUG_MAC_MAGIC 0x4D414350 // "MACP" in hex`

- `#define BRIGHTNESS_MAGIC 0x54494C42 // "BLIT" in hex`

- `#define TFT_BACKLIGHT_PIN 27`

- `#define BACKLIGHT_PWM_CHANNEL 7`

- `#define BACKLIGHT_PWM_FREQ 5000`

- `#define BACKLIGHT_PWM_RESOLUTION 8`

- `#define BUTTON_LIVE_X 10`

- `#define BUTTON_LIVE_Y 10`

- `#define BUTTON_LIVE_W 60`

- `#define BUTTON_LIVE_H 30`

- `#define BUTTON_GRAPH_X 80`

- `#define BUTTON_GRAPH_Y 10`

- `#define BUTTON_GRAPH_W 60`

- `#define BUTTON_GRAPH_H 30`

- `#define BUTTON_CALIB_X 150`

- `#define BUTTON_CALIB_Y 10`

- `#define BUTTON_CALIB_W 80`

- `#define BUTTON_CALIB_H 30`

- `#define BUTTON_CONTINUE_X 60`

- `#define BUTTON_CONTINUE_Y 140`

- `#define BUTTON_CONTINUE_W 100`

- `#define BUTTON_CONTINUE_H 40`

- `#define MANAGE_BREW_ROW_Y 184`

- `#define MANAGE_BREW_ROW_H 34`

- `#define MANAGE_BREW_NAV_W 48`

- `#define MANAGE_BREW_ACTION_W 90`

- `#define MANAGE_BREW_PREV_X MARGIN`

- `#define MANAGE_BREW_CONT_X(MARGIN + 60) #define MANAGE_BREW_COPY_X (MARGIN + 162)`

- `#define MANAGE_BREW_DELETE_X(MARGIN + 264) #define MANAGE_BREW_NEXT_X (UI_W - MARGIN - MANAGE_BREW_NAV_W)`

- `#define BUTTON_NEW_X 160`

- `#define BUTTON_NEW_Y 140`

- `#define BUTTON_NEW_W 100`

- `#define BUTTON_NEW_H 40`

- `#define BUTTON_DIALOG_YES_X 80`

- `#define BUTTON_DIALOG_YES_Y 180`

- `#define BUTTON_DIALOG_YES_W 100`

- `#define BUTTON_DIALOG_YES_H 40`

- `#define BUTTON_DIALOG_BACK_X 220`

- `#define BUTTON_DIALOG_BACK_Y 180`

- `#define BUTTON_DIALOG_BACK_W 100`

- `#define BUTTON_DIALOG_BACK_H 40`

- `#define BUTTON_CALIB_START_X(UI_W - MARGIN - 132) #define BUTTON_CALIB_START_Y 236`

- `#define BUTTON_CALIB_START_W 132`

- `#define BUTTON_CALIB_START_H 36`

- `#define BUTTON_CALIB_BACK_X MARGIN`

- `#define BUTTON_CALIB_BACK_Y 236`

- `#define BUTTON_CALIB_BACK_W 132`

- `#define BUTTON_CALIB_BACK_H 36`

- `#define BUTTON_CALIB_NEXT_X(UI_W - MARGIN - 132) #define BUTTON_CALIB_NEXT_Y 236`

- `#define BUTTON_CALIB_NEXT_W 132`

- `#define BUTTON_CALIB_NEXT_H 36`

- `#define BUTTON_CALIB_APPLY_X(UI_W - MARGIN - 132) #define BUTTON_CALIB_APPLY_Y 236`

- `#define BUTTON_CALIB_APPLY_W 132`

- `#define BUTTON_CALIB_APPLY_H 36`

- `#define BUTTON_CALIB_EXIT_X MARGIN`

- `#define BUTTON_CALIB_EXIT_Y 236`

- `#define BUTTON_CALIB_EXIT_W 132`

- `#define BUTTON_CALIB_EXIT_H 36`

- `#define BUTTON_CALIB_OFFSET_X(UI_W - MARGIN - 132) #define BUTTON_CALIB_OFFSET_Y 236`

- `#define BUTTON_CALIB_OFFSET_W 132`

- `#define BUTTON_CALIB_OFFSET_H 36`

- `#define BUTTON_CALIB_RECORD_X(UI_W - MARGIN - 132) #define BUTTON_CALIB_RECORD_Y 236`

- `#define BUTTON_CALIB_RECORD_W 132`

- `#define BUTTON_CALIB_RECORD_H 36`

- `#define BATTERY_BUTTON_H 40`

- `#define BATTERY_BUTTON_Y(UI_H - NAV_H - BATTERY_BUTTON_H - GAP) // Exit button for Step 1/5 - lower left`

- `#define BUTTON_CALIB_EXIT_OFFSET_X MARGIN`

- `#define BUTTON_CALIB_EXIT_OFFSET_Y 236`

- `#define BUTTON_CALIB_EXIT_OFFSET_W 132`

- `#define BUTTON_CALIB_EXIT_OFFSET_H 36`

- `#define BUTTON_CALIB_SKIP_X MARGIN`

- `#define BUTTON_CALIB_SKIP_Y 236`

- `#define BUTTON_CALIB_SKIP_W 132`

- `#define BUTTON_CALIB_SKIP_H 36`

- `#define BUTTON_CALIB_SKIP_YES_X(UI_W - MARGIN - 132) #define BUTTON_CALIB_SKIP_YES_Y 236`

- `#define BUTTON_CALIB_SKIP_YES_W 132`

- `#define BUTTON_CALIB_SKIP_YES_H 36`

- `#define BUTTON_CALIB_SKIP_NO_X MARGIN`

- `#define BUTTON_CALIB_SKIP_NO_Y 236`

- `#define BUTTON_CALIB_SKIP_NO_W 132`

- `#define BUTTON_CALIB_SKIP_NO_H 36`

- `#define GRAPH_BUTTON_H 40`

- `#define GRAPH_BUTTON_Y(UI_H - NAV_H - GRAPH_BUTTON_H - GAP) int contentY = TOPBAR_H + MARGIN;`

## Key Dependencies

**Local:**
- `SGNode_Base/src/assets/Logo2_Optimized.h`
- `SGNode_Base/src/ui/ui_tokens.h`
- `SGNode_Base/src/ui/ui_components.h`
- `SGNode_Base/src/domain/brew_profile.h`
- `SGNode_Base/src/ui/brew_wizard_controller.h`
- `SGNode_Base/src/calculations/brix_converter.h`
- `SGNode_Base/src/calculations/og_verifier.h`
- `SGNode_Base/src/calculations/derived_calculations.h`
- `SGNode_Base/src/calculations/target_curve.h`
- `SGNode_Base/src/domain/fermentation_state_machine.h`

**System/External:**
- `<WiFi.h>`
- `<esp_now.h>`
- `<WebServer.h>`
- `<Update.h>`
- `<TFT_eSPI.h>`
- `<FS.h>`
- `<SD.h>`
- `<SPI.h>`
- `<Wire.h>`
- `<EEPROM.h>`

## Side Effects & Hardware Access

- ESP-NOW packet transmission
- ESP-NOW packet reception
- ESP-NOW peer registration
- EEPROM write (persistent storage)
- EEPROM read
- EEPROM initialization
- EEPROM commit (flush)
- SD card write (CSV/JSON)
- SD card read
- SD card file operations
- Relay control (fridge compressor)
- Sensor reading
- Touchscreen UI updates
- SPI bus communication
- Temperature target calculation
- Calibration coefficient computation
- Calibration data persistence
- WiFi mode configuration
- WiFi access point mode
- Execution delay
- Serial port initialization
- Serial output

## Change Risks

- **MEDIUM**: Core functionality change

---
*Last modified (hash): 13bce0a2e95b785e0de0d517c402f774edc6f345c7a4d577b6123963380441d2*