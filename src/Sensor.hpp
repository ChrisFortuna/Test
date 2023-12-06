#ifndef SENSOR_H
#define SENSOR_H

#include <string>
#include <vector>

using std::string;
using std::vector;



class Sensor{
	public:
	Sensor(string name, unsigned short pin);
	Sensor();

//setup sensor methods:
	void all_sensor_bleWrite();
	void allSensorsFlashWrite();
	void allSensorsToService();
	
//update sensor methods:	
	int16_t sensor_read();
	void sensor_update();

//getters
	const vector <Sensor *>& get_sensor_tab();
	BLEService& get_sensorService();
	string get_name();
	int16_t get_last_value();


  protected:
  	string name;
	unsigned short pin;
    	int16_t last_value;
	 
	static vector<Sensor *> sensor_tab;
      static BLEService* sensorService;
	static string descriptor;
};

void add_sensorService();
void advertise_sensorService();
void allSensorSetup();
void allSensorPrint();
void allSensorRecordViaFlash();
void allSensorRecordViaBle();
void resetTimeReference();
void sendBufferViaBLE(uint8_t* tempBuff, int length);
uint8_t getSensorNb();

#endif
