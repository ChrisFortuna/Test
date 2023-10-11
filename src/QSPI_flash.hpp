#ifndef QSPI_FLASH_H
#define QSPI_FLASH_H

#include "Macros.hpp"

void  init_flash();
void  flash_set_sensor_nb(short sensor_nb);
bool  write_sensor_data_line(uint16_t buff[]);
void  test_flash();
void  read_all_sensor_data();
void  read_one_line(uint32_t data_point, uint16_t buff[]);
void  reset_flash_sensor_data_ptr();
bool  reset_flash();
bool  check_if_flash_free();

void  readFlashInfo(uint16_t buff[2]);

#endif