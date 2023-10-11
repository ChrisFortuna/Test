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

uint32_t Sensor::dataCount(0);
string Sensor::descriptor="";
static u_int16_t buff[MAX_PINS+2]={0};

static uint32_t time_reference(0);

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
	uint8_t buff[sensor_tab.size()*2];
	for(unsigned i(0); i<sensor_tab.size(); ++i){ //big endian
		buff[2*i] = sensor_tab[i]->last_value>>8 & 0x00FF; 
		buff[2*i+1] = sensor_tab[i]->last_value & 0x00FF;
	}

	ble_characteristic->writeValue(buff,sensor_tab.size()*2);
}
void Sensor::allSensorsFlashWrite(){
	unsigned tab_size=sensor_tab.size();

	uint32_t timeTaken(millis()-time_reference);
	buff[0] = uint16_t(timeTaken>>16 & 0x0000FFFF);
	buff[1] = uint16_t(timeTaken & 0x0000FFFF);
	for(unsigned i(0);i<tab_size;++i){
		buff[i+2]=sensor_tab[i]->last_value;
	} 

	write_sensor_data_line(buff);
}
void Sensor::all_sensor_to_service(){
	ble_characteristic = new BLECharacteristic (SENSOR_CHAR_UUID,BLEWrite | BLENotify,20); //affectation operator was not defined
	for(auto const& sensor_addr: sensor_tab){
		descriptor+=sensor_addr->name + "#";
	}
	descriptor.pop_back();
	Serial.println(descriptor.c_str());

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
void Sensor::increment_dataCount(){
	++dataCount;
}
uint32_t Sensor::get_dataCount(){
	return dataCount;
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
void all_sensor_setup(){
	Sensor dummy;
	dummy.all_sensor_to_service();
	flash_set_sensor_nb(dummy.get_sensor_tab().size());
}
void all_sensor_print(){
	Sensor dummy;
	for(auto const& sensor_addr: dummy.get_sensor_tab()){
		const char* msg1 = ("Sensor "+ sensor_addr->get_name() + "| last value: " 
		+ std::to_string(sensor_addr->get_last_value())).c_str();
		Serial.println(msg1);
		delete msg1;
	} 
}
void all_sensor_record(){
	Sensor dummy;
	for(auto const& sensor_addr: dummy.get_sensor_tab()){
		sensor_addr->sensor_update();
	}
	dummy.allSensorsFlashWrite();
	dummy.increment_dataCount();
	read_all_sensor_data();
}
void all_sensor_ble_send(){
	Sensor dummy;
	uint16_t size(dummy.get_sensor_tab().size());
	int8_t tempBuff[size*2]={0};

	Serial.print("Sensor reads: ");
	for(uint16_t i(0);i<size;++i){
		int16_t tst=dummy.get_sensor_tab()[i]->sensor_read();
		Serial.println(tst);
		tempBuff[2*i] = byte((tst>>16) & 0x00FF);
		tempBuff[2*i+1] = byte(tst & 0x00FF);
	}
	ble_characteristic->writeValue(tempBuff,size*2);
}
void flashBleSend(){
      Sensor dummy;
	unsigned tab_size = dummy.get_sensor_tab().size() + 2; //+2 because timestamp
	uint16_t infoBuff[2] ={0};
	readFlashInfo(infoBuff);

	uint16_t buff[] ={0};
	uint16_t flashDataCount = infoBuff[0];

	uint8_t tempBuff0[2] ={0};
	tempBuff0[0] = (byte)((flashDataCount>>16) & 0x00FF);
	tempBuff0[1] = (byte) (flashDataCount & 0x00FF);

	ble_characteristic->writeValue(tempBuff0, 2);


	if (flashDataCount != dummy.get_dataCount()) {
		Serial.println("Inconsistency in data counting!");
	}
	
	uint8_t tempBuff[tab_size*2]={0};
	for(uint32_t i(0);  i<flashDataCount; ++i){
		read_one_line(i,buff);
		tempBuff[0] = (byte)((buff[0]>>16) & 0x00FF);
		tempBuff[1] = (byte)(buff[0] & 0x00FF);

		tempBuff[2] = (byte)((buff[1]>>16) & 0x00FF);
		tempBuff[3] = (byte)(buff[1] & 0x00FF);

		for(unsigned i(2); i<tab_size;++i){
			tempBuff[2*i] = (byte)((buff[i]>>16) & 0x00FF);
			Serial.println(tempBuff[2*i]);
			Serial.println(tempBuff[2*i+1]);
			tempBuff[2*i+1] = (byte)(buff[i] & 0x00FF);
		}
		ble_characteristic->writeValue(tempBuff, tab_size*2);
		delay(100);
	}

}
unsigned get_sensor_nb(){
	Sensor dummy;
	return dummy.get_sensor_tab().size();
}
void reset_time_reference(){
	time_reference=millis();
}