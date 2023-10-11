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
	void all_sensor_to_service();
	
//update sensor methods:	
	int16_t sensor_read();
	void sensor_update();
	static void increment_dataCount();

//getters
	const vector <Sensor *>& get_sensor_tab();
	BLEService& get_sensorService();
	string get_name();
	int16_t get_last_value();
	uint32_t get_dataCount();


  protected:
  	string name;
	unsigned short pin;
    	int16_t last_value;
	 
	static uint32_t dataCount;
	static vector<Sensor *> sensor_tab;
      static BLEService* sensorService;
	static string descriptor;
};

void add_sensorService();
void advertise_sensorService();
void all_sensor_setup();
void all_sensor_print();
void all_sensor_record();
void flashBleSend();
void all_sensor_ble_send();
unsigned get_sensor_nb();
void reset_time_reference();

#endif
