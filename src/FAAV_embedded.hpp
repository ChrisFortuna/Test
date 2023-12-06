#ifndef FAV_MAIN_H
#define FAV_MAIN_H
// Contains definition of functions
#include <Arduino.h>

// #define _TASK_TIMECRITICAL      // Enable monitoring scheduling overruns
#define _TASK_SLEEP_ON_IDLE_RUN // Enable 1 ms SLEEP_IDLE powerdowns between tasks if no callback methods were invoked during the pass
//#define _TASK_STATUS_REQUEST    // Compile with support for StatusRequest functionality - triggering tasks on status change events in addition to time only
// #define _TASK_WDT_IDS           // Compile with support for wdt control points and task ids
// #define _TASK_LTS_POINTER       // Compile with support for local task storage pointer
// #define _TASK_PRIORITY          // Support for layered scheduling priority
// #define _TASK_MICRO_RES         // Support for microsecond resolution
// #define _TASK_STD_FUNCTION      // Support for std::function (ESP8266 and ESP32 ONLY)
// #define _TASK_DEBUG             // Make all methods and variables public for debug purposes
// #define _TASK_INLINE            // Make all methods "inline" - needed to support some multi-tab, multi-file implementations
// #define _TASK_TIMEOUT           // Support for overall task timeout
// #define _TASK_OO_CALLBACKS      // Support for dynamic callback method binding

#define DISCONNECTED 0
#define CONNECTED 1
#define FLAPPING 2


#define MISC_SERVICE "13012F00-F8C3-4F4A-A8F4-15CD926DA146"
#define UUID_mode "13012F03-F8C3-4F4A-A8F4-15CD926DA147"
#define UUID_RPYT "13012F03-F8C3-4F4A-A8F4-15CD926DA148"


#define UUID_ParamSequence "13012F03-F8C3-4F4A-A8F4-15CD926DA153"
#define UUID_RunSequence "13012F03-F8C3-4F4A-A8F4-15CD926DA154"
#define UUID_ValueSequence "13012F03-F8C3-4F4A-A8F4-15CD926DA155"

struct rpyt_struct{             // Structure declaration
  int roll;         
  int pitch;
  int yaw;
  int thrust;   
};     
void resetRobot();

void builtinLED(char color);
void manual_flight_controller(struct rpyt_struct rpyt);
void modeChangeHandler(int mode);
void onBLEConnected(BLEDevice central);
void onBLEDisconnected(BLEDevice central);
void initBle();
void initTaskScheduler();
void initGPIOs();
void setupMiscService();
void flapping_active(bool active);
void wing_sweep(int sweep);

///////////////////////////////////////////////////////////////////////////////////////////
// Task Callbacks:
void t1Callback();
void t1offCallback();
void tFlappingONCallback();
void tFlappingOFFCallback();
void tSensorsCallback();
void tFlashSendCallback();

void BleCommandCallback(BLEDevice central, BLECharacteristic characteristic);

#endif