#ifndef QSPI_FLASH_H
#define QSPI_FLASH_H

#include "Macros.hpp"

void  initFlash();
void  flashSetSensorNb(short sensor_nb);
void  writeSensorDataLine(uint16_t buff[]);
void  readAllSensorFlash();
void  readOneLine(uint32_t data_point, uint16_t buff[]);
void  resetFlash();
bool  checkIfFlashFree();

void  readFlashInfo(uint16_t buff[2]);
void  printAllSensorData();
#endif