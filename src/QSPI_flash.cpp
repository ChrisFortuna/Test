#include <ArduinoBle.h>

#include <stdio.h>
#include <vector>
#include <string.h>
#include <stdlib.h>
#include "nrfx_qspi.h"
#include "app_util_platform.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
#include "sdk_config.h"
#include "nrf_delay.h"
#include "avr/interrupt.h"

#include "Macros.hpp"


// QSPI Settings
#define QSPI_STD_CMD_WRSR     0x01
#define QSPI_STD_CMD_RSTEN    0x66
#define QSPI_STD_CMD_RST      0x99
#define QSPI_DPM_ENTER        0x0003 // 3 x 256 x 62.5ns = 48ms
#define QSPI_DPM_EXIT         0x0003

#define FLASH_SENSOR_INIT_VAL 0x00000004


static uint32_t               *QSPI_Status_Ptr = (uint32_t*) 0x40029604;  // Setup for the SEEED XIAO BLE - nRF52840
static nrfx_qspi_config_t     QSPIConfig;
static nrf_qspi_cinstr_conf_t QSPICinstr_cfg;// Alter this to create larger read writes, 64Kb is the size of the Erase
static bool                   QSPIWait = false;

static unsigned short         sensorCount(0);
static uint32_t               flashSensorPointer(FLASH_SENSOR_INIT_VAL); 
static uint32_t               batchSize(0);

#define SENSOR_NB_BUFFER      13 //max analog pins XIAO - nRF52840
#define WORD_SIZE             2 //2 bytes for int16_t
#define DELIMITER_SIZE        4
#define TIMESTAMP_SIZE        2
// QSPI Settings Complete

///////////////////////////////////////////////////////////////////////////////////////////
// QSPI generic functions:
static void qspi_handler(nrfx_qspi_evt_t event, void *p_context) {
      // UNUSED_PARAMETER(p_context);
      // 
      if(DEBUG_ON) Serial.println("QSPI Interrupt");
      // if (event == NRFX_QSPI_EVENT_DONE) {
      //       QSPI_HasFinished = true;
      // }
}

static nrfx_err_t QSPI_IsReady() {
      if (((*QSPI_Status_Ptr & 8) == 8) && (*QSPI_Status_Ptr & 0x01000000) == 0) {
            return NRFX_SUCCESS;  
      } else {
            return NRFX_ERROR_BUSY; 
      }
}

static void QSPI_Status(char ASender[]) { 
      // Prints the QSPI Status
      if(DEBUG_ON){
            Serial.print("(");
            Serial.print(ASender);
            Serial.print(") QSPI is busy/idle ... Result = ");
            Serial.println(nrfx_qspi_mem_busy_check() & 8);
            Serial.print("(");
            Serial.print(ASender);
            Serial.print(") QSPI Status flag = 0x");
            Serial.print(NRF_QSPI->STATUS, HEX);
            Serial.print(" (from NRF_QSPI) or 0x");
            Serial.print(*QSPI_Status_Ptr, HEX);
            Serial.println(" (from *QSPI_Status_Ptr)");
      }
}

static nrfx_err_t QSPI_WaitForReady() {
      while (QSPI_IsReady() == NRFX_ERROR_BUSY) {
            if (DEBUG_ON) {
                  Serial.print("*QSPI_Status_Ptr & 8 = ");
                  Serial.print(*QSPI_Status_Ptr & 8);
                  Serial.print(", *QSPI_Status_Ptr & 0x01000000 = 0x");
                  Serial.println(*QSPI_Status_Ptr & 0x01000000, HEX);
                  QSPI_Status("QSPI_WaitForReady");
            }   
      }
      return NRFX_SUCCESS;
}

static void QSIP_Configure_Memory() {
      // uint8_t  temporary = 0x40;
      uint8_t  temporary[] = {0x00, 0x02};
      uint32_t Error_Code;
      
      QSPICinstr_cfg = {
            .opcode    = QSPI_STD_CMD_RSTEN,
            .length    = NRF_QSPI_CINSTR_LEN_1B,
            .io2_level = true,
            .io3_level = true,
            .wipwait   = QSPIWait,
            .wren      = true
      };
      QSPI_WaitForReady();
      
      if (nrfx_qspi_cinstr_xfer(&QSPICinstr_cfg, NULL, NULL) != NRFX_SUCCESS) { // Send reset enable
            if (DEBUG_ON){
                  Serial.println("(QSIP_Configure_Memory) QSPI 'Send reset enable' failed!");
            }
      } else {
            QSPICinstr_cfg.opcode = QSPI_STD_CMD_RST;
            QSPI_WaitForReady();
            if (nrfx_qspi_cinstr_xfer(&QSPICinstr_cfg, NULL, NULL) != NRFX_SUCCESS) { // Send reset command
                  if (DEBUG_ON){
                        Serial.println("(QSIP_Configure_Memory) QSPI Reset failed!");
                  }
            } else {
                  QSPICinstr_cfg.opcode = QSPI_STD_CMD_WRSR;
                  QSPICinstr_cfg.length = NRF_QSPI_CINSTR_LEN_3B;
                  QSPI_WaitForReady();
                  if (nrfx_qspi_cinstr_xfer(&QSPICinstr_cfg, &temporary, NULL) != NRFX_SUCCESS) { // Switch to qspi mode
                        if (DEBUG_ON){
                              Serial.println("(QSIP_Configure_Memory) QSPI failed to switch to QSPI mode!");
                        }
                  } else {
                        QSPI_Status("QSIP_Configure_Memory");
                  }
            }
      }
}

static nrfx_err_t QSPI_Initialise() { // Initialises the QSPI and NRF LOG
      uint32_t Error_Code;

      NRF_LOG_INIT(NULL); // Initialise the NRF Log
      NRF_LOG_DEFAULT_BACKENDS_INIT();
      // QSPI Config
      QSPIConfig.xip_offset = NRFX_QSPI_CONFIG_XIP_OFFSET;                       
      QSPIConfig.pins = { // Setup for the SEEED XIAO BLE - nRF52840                                                     
            .sck_pin     = 21,                                
            .csn_pin     = 25,                                
            .io0_pin     = 20,                                
            .io1_pin     = 24,                                
            .io2_pin     = 22,                                
            .io3_pin     = 23,                                
      };                                                                  
      QSPIConfig.irq_priority = (uint8_t)NRFX_QSPI_CONFIG_IRQ_PRIORITY;           
      QSPIConfig.prot_if = {                                                        
            // .readoc     = (nrf_qspi_readoc_t)NRFX_QSPI_CONFIG_READOC,
            .readoc     = (nrf_qspi_readoc_t)NRF_QSPI_READOC_READ4O,       
            // .writeoc    = (nrf_qspi_writeoc_t)NRFX_QSPI_CONFIG_WRITEOC,     
            .writeoc    = (nrf_qspi_writeoc_t)NRF_QSPI_WRITEOC_PP4O,
            .addrmode   = (nrf_qspi_addrmode_t)NRFX_QSPI_CONFIG_ADDRMODE,   
            .dpmconfig  = false,                                            
      };                   
      QSPIConfig.phy_if.sck_freq   = (nrf_qspi_frequency_t)NRF_QSPI_FREQ_32MDIV1;  // I had to do it this way as it complained about nrf_qspi_phy_conf_t not being visible                                        
      // QSPIConfig.phy_if.sck_freq   = (nrf_qspi_frequency_t)NRFX_QSPI_CONFIG_FREQUENCY; 
      QSPIConfig.phy_if.spi_mode   = (nrf_qspi_spi_mode_t)NRFX_QSPI_CONFIG_MODE;
      QSPIConfig.phy_if.dpmen      = false;
      // QSPI Config Complete
      // Setup QSPI to allow for DPM but with it turned off
      QSPIConfig.prot_if.dpmconfig = true;
      NRF_QSPI->DPMDUR = (QSPI_DPM_ENTER << 16) | QSPI_DPM_EXIT; // Found this on the Nordic Q&A pages, Sets the Deep power-down mode timer
      Error_Code = 1;
      while (Error_Code != 0) {
            Error_Code = nrfx_qspi_init(&QSPIConfig, NULL, NULL);           
            if (Error_Code != NRFX_SUCCESS) {
                  if (DEBUG_ON) {
                        Serial.print("(QSPI_Initialise) nrfx_qspi_init returned : ");
                        Serial.println(Error_Code);
                  }
            } else {
                  if (DEBUG_ON) {
                        Serial.println("(QSPI_Initialise) nrfx_qspi_init successful");
                  }
            }
      }

      QSPI_Status("QSPI_Initialise (Before QSIP_Configure_Memory)");
      QSIP_Configure_Memory();
      if (DEBUG_ON) {
            Serial.println("(QSPI_Initialise) Wait for QSPI to be ready ...");
      }
      NRF_QSPI->TASKS_ACTIVATE = 1;
      QSPI_WaitForReady();
      if (DEBUG_ON) {
            Serial.println("(QSPI_Initialise) QSPI is ready");
      }

      return QSPI_IsReady(); 
}

static bool QSPI_Erase(uint32_t AStartAddress) {
      uint32_t   TimeTaken(millis());
      bool       QSPIReady = false;
      bool       AlreadyPrinted = false;

      if (DEBUG_ON) {
            Serial.println("(QSPI_Erase) Erasing memory");
      }
      while (!QSPIReady) {
            if (QSPI_IsReady() != NRFX_SUCCESS) {
                  if (!AlreadyPrinted) {
                        QSPI_Status("QSPI_Erase (Waiting)");
                        AlreadyPrinted = true;
                  }
            } else {
                  QSPIReady = true;
                  QSPI_Status("QSPI_Erase (Waiting Loop Breakout)");
            }
      }
      if (nrfx_qspi_erase(NRF_QSPI_ERASE_LEN_ALL, AStartAddress) != NRFX_SUCCESS) {
            if(DEBUG_ON){
                  Serial.print("(QSPI_Initialise_Page) QSPI Address 0x");
                  Serial.print(AStartAddress, HEX);
                  Serial.println(" failed to erase!");
            }
            return 0;
      } else {     
            TimeTaken = millis() - TimeTaken;
            if(DEBUG_ON){
                  Serial.print("(QSPI_Initialise_Page) QSPI took ");
                  Serial.print(TimeTaken);
                  Serial.println("ms to erase all memory");
            }
            return 1;
      }
}


///////////////////////////////////////////////////////////////////////////////////////////
// QSPI show data functions:
static void printData(uint16_t *AnAddress, uint32_t AnAmount) {
      Serial.print("Data :\n");
      for (uint32_t i = 0; i < AnAmount; i++) {
            Serial.println(*(AnAddress + i));
      }
      Serial.println("");
}
void printAllSensorData(){
      uint16_t buff[(flashSensorPointer)/WORD_SIZE]; //+2 because of the first two elements
      QSPI_WaitForReady();
      nrfx_qspi_read(buff, flashSensorPointer, 0x00000000);

      Serial.println("######################");
      Serial.print("Number of data points:");
      Serial.println(buff[0]);
      Serial.print("Flash extra info: ");
      Serial.println(buff[1]);
      
      uint32_t timeStamp (0);
      //Serial.println(flashSensorPointer);
      for (uint32_t i = 2; i < flashSensorPointer/WORD_SIZE; i++) {

            switch ((i-2) % (batchSize/WORD_SIZE)){
                  case 0:
                        timeStamp = buff[i]<<16;
                        break;
                  case 1:
                        timeStamp += buff[i];
                        Serial.print("Millis value: ");
                        Serial.println(timeStamp);
                        break;
                  default:
                        Serial.print(buff[i]);
                        if((i-2) % (batchSize/WORD_SIZE) != batchSize/WORD_SIZE-1){
                              Serial.print(", ");
                        }
                        else Serial.println("");
            }
      }

}

void resetFlash(){
//reset flash pointer
      flashSensorPointer=FLASH_SENSOR_INIT_VAL;

//reset flash info in the flash memory
      uint16_t counterBuff[2] = {0};
      nrfx_qspi_write(counterBuff,4,0x00000000);
}
void initFlash() {
      //Make sure that the flash knows how many sensors the system has! Do flashSetSensorNb before
      // doing initFLash
      if (QSPI_Initialise() != NRFX_SUCCESS) {
            if(DEBUG_ON) Serial.println("(Setup) QSPI Memory failed to start!");
      } else {
            if(DEBUG_ON) Serial.println("(Setup) QSPI initialised and ready");
            QSPI_Status("Setup (After initialise)");
      }
      
      uint16_t counterBuff[2] = {0};
      nrfx_qspi_read(counterBuff,4,0x00000000);
      
      flashSensorPointer =  FLASH_SENSOR_INIT_VAL + counterBuff[0]*batchSize;
      
      QSPI_WaitForReady();
      QSPI_Status("Setup");
}
void flashSetSensorNb(short sensor_nb){
      sensorCount=sensor_nb;
      if(sensor_nb % 2 ==0) batchSize=(sensorCount+TIMESTAMP_SIZE)*WORD_SIZE;
      else batchSize=(sensorCount+TIMESTAMP_SIZE+1)*WORD_SIZE;
}
uint8_t getBatchSize(){
      return batchSize;
}
void writeSensorDataLine(uint16_t buff[]){
      uint16_t counterBuff[2] = {0};
      nrfx_qspi_read(counterBuff,4,0x00000000);
      
      counterBuff[0] = ++counterBuff[0];
      counterBuff[1] = 0;

      QSPI_WaitForReady();
      nrfx_qspi_write(counterBuff,4,0x00000000);

      QSPI_WaitForReady();

      nrfx_qspi_write(buff, batchSize, flashSensorPointer); 
      flashSensorPointer+=batchSize;
}
void readAllSensorFlash(){
      //not really tested as it was not needed
      // uint16_t buff[(flashSensorPointer)/WORD_SIZE]; 
      // QSPI_WaitForReady();
      // nrfx_qspi_read(buff, flashSensorPointer, 0x00000000);
      // printData(buff,flashSensorPointer/WORD_SIZE);
}
void readFlashInfo(uint16_t buff[2]){
      nrfx_qspi_read(buff,4, 0x00000000);
}
void readOneLine(uint32_t data_point, uint16_t buff[]){
      nrfx_qspi_read(buff,batchSize, data_point*batchSize+4); // +4 to skip flashInfo
}

bool checkIfFlashFree(){
      return (QSPI_IsReady() == NRFX_SUCCESS);
}


