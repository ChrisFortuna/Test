#include <ArduinoBle.h>
#include <string>
#include <vector>
#include <array>
#include <algorithm>

#include "Macros.hpp"
#include "QSPI_flash.hpp"
#include "Sensor.hpp"

#define SENSOR_SERVICE_UUID 	"c195183b-53c0-425c-93cd-9f7bdf5027bb"
#define SENSOR_CHAR_UUID "c195183b-53c0-425c-93cd-9f7bdf5027b0"

using std::string;
using std::vector;

string Sensor::descriptor="";

static uint32_t timeReference(0);

static BLECharacteristic* ble_characteristic;
static BLEDescriptor* ble_descriptor;

Sensor::Sensor(string my_name, unsigned short my_pin)
{
      if(!sensor_tab.size()){
            sensorService = new BLEService(SENSOR_SERVICE_UUID);
      }	
	
	sensor_tab.push_back(this);
	
	last_value = 0;
	pin=my_pin;
	name=my_name;
}
Sensor::Sensor()
{}


void Sensor::all_sensor_bleWrite(){
	///this needs fixing
	uint8_t buff[sensor_tab.size()*2];
	for(unsigned i(0); i<sensor_tab.size(); ++i){ //big endian
		buff[2*i] = sensor_tab[i]->last_value>>8 & 0x00FF; 
		buff[2*i+1] = sensor_tab[i]->last_value & 0x00FF;
	}

	ble_characteristic->writeValue(buff,sensor_tab.size()*2);
}
void Sensor::allSensorsFlashWrite(){
	unsigned tabSize=sensor_tab.size();
	uint8_t buffSize = 0;
//flash memory has 32bit words. writing in less than 32 bit will cause weird behaviour
	if(tabSize%2==0){
		buffSize = tabSize+2;
	}else{
		buffSize = tabSize+3;
	}

	uint16_t buff[buffSize] = {0};
	if(tabSize%2!=0) buff[tabSize+2] = 0x1111;


	uint32_t timeTaken(millis()-timeReference);
	//big endian encoding
	buff[0] = uint16_t(timeTaken >> 16 & 0x0000FFFF);
	buff[1] = uint16_t(timeTaken & 0x0000FFFF);
	if(DEBUG_ON) {
		Serial.print("Sensors record at time: ");
		Serial.print(buff[0]);
		Serial.print("_");
		Serial.println(buff[1]);
	}
	for(unsigned i(0);i<tabSize;++i){
		buff[i+2]=sensor_tab[i]->last_value;
		if(DEBUG_ON){
			Serial.print(buff[i+2]);
			if(i==tabSize-1) Serial.println("");
			else Serial.print(", ");
		}
	} 
	
	writeSensorDataLine(buff);
}
void Sensor::allSensorsToService(){
	ble_characteristic = new BLECharacteristic (SENSOR_CHAR_UUID,BLEWrite | BLENotify,20); //affectation operator was not defined
	for(auto const& sensor_addr: sensor_tab){
		descriptor+=sensor_addr->name + "#";
	}
	descriptor.pop_back();
	if(DEBUG_ON) Serial.println(descriptor.c_str());

	ble_descriptor = new BLEDescriptor("2901", descriptor.c_str());
	
	ble_characteristic->addDescriptor(*ble_descriptor);
	sensorService->addCharacteristic(*ble_characteristic);
}
const vector <Sensor *>& Sensor::get_sensor_tab(){
	return sensor_tab;
}

BLEService& Sensor::get_sensorService(){
	return *sensorService;
}
int16_t Sensor::sensor_read(){
	return analogRead(pin);
}
void Sensor::sensor_update(){
	last_value = analogRead(pin);
}
string Sensor::get_name(){
	return name;
}
int16_t Sensor::get_last_value(){
	return last_value;
}

// functions
void advertise_sensorService(){
	Sensor dummy;
	BLE.setAdvertisedService(dummy.get_sensorService());
}
void add_sensorService(){
	Sensor dummy;
	BLE.addService(dummy.get_sensorService());
}
void allSensorSetup(){
	Sensor dummy;
	dummy.allSensorsToService();
	flashSetSensorNb(dummy.get_sensor_tab().size());
}
void allSensorPrint(){
	Sensor dummy;
	for(auto const& sensor_addr: dummy.get_sensor_tab()){
		const char* msg1 = ("Sensor "+ sensor_addr->get_name() + "| last value: " 
		+ std::to_string(sensor_addr->get_last_value())).c_str();
		Serial.println(msg1);
		delete msg1;
	} 
}
void allSensorRecordViaFlash(){
	Sensor dummy;
	for(auto const& sensor_addr: dummy.get_sensor_tab()){
		sensor_addr->sensor_update();
	}
	dummy.allSensorsFlashWrite();
	if(DEBUG_ON) printAllSensorData();
}
void allSensorRecordViaBle(){
	Sensor dummy;
	uint16_t size(getSensorNb());
	int8_t tempBuff[size*2]={0};

	if(DEBUG_ON) Serial.println("Sensors read: ");
	for(uint16_t i(0);i<size;++i){
		int16_t tst= dummy.get_sensor_tab()[i]->sensor_read();
		
		//big endian encoding
		tempBuff[2*i] = byte((tst>>8) & 0x00FF);
		tempBuff[2*i+1] = byte(tst & 0x00FF);
		if(DEBUG_ON){
			Serial.print(tst);
			if(i==size-1) Serial.println("");
			else Serial.print(", ");
		}
	}
	ble_characteristic->writeValue(tempBuff,size*2);
}
uint8_t getSensorNb(){
	Sensor dummy;
	return dummy.get_sensor_tab().size();
}
void resetTimeReference(){
	timeReference=millis();
}
void sendBufferViaBLE(uint8_t* tempBuff, int length){
	ble_characteristic->writeValue(tempBuff, length);
}
