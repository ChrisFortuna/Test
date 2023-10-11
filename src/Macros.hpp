#ifndef MACROS_H
#define MACROS_H

#define MAX_PINS        11
#define DELIMITER       0xFFFF
// Pins
#define PIN_VSENSE      0
#define PIN_ISENSE      1
#define PIN_WSENSE      2
#define PIN_ESC         6
#define PIN_WINGSERVO_R 5
#define PIN_WINGSERVO_L 4
#define PIN_ELE_SERVO   3

// BLE 
# define PARAM_DGCNT    4 // very tempory way of generating uuid. Really bad. Replace as soon as possible
# define DEVICE_NAME    "Seeed XIAO nRF58240 BLE"
#define MAX_BAD_FLASH_READS 20

// Modes
#define RC_MODE         0
#define PARAM_MODE      1
#define SEQUENCE        2

#define COMMAND_UUID "0592b69b-087f-47ba-9112-e37c8ba29281"
#define MODE_UUID "054c764d-70b6-4952-8576-7a04b11b08af"
#define RPYT_UUID "655c8dfe-5940-11ee-8c99-0242ac120002"
#define STATE_UUID "2c30071b-312c-4f60-bdf5-2119c0529cb4"

#endif