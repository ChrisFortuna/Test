#include <ArduinoBle.h>
#include <string>
#include <vector>
#include <algorithm>

#include <Macros.hpp>
#include <Parameter.hpp>

#define PARAM_SERVICE_UUID "889e60ca-aa43-4b44-abdb-3d98ca8d8d87"

using std::string;
using std::vector;

static string to_string_fixed(int value, unsigned str_size){
	string result;
	while (str_size-- > 0) {
		result += ('0' + value % 10);
		value /= 10;
	}
	reverse(result.begin(), result.end());
	return result;
}


Parameter::Parameter(string name, int16_t val, PARAM_TYPE param_type,  string info)
{
      if(!param_tab.size()){
            paramService = new BLEService(PARAM_SERVICE_UUID);
      }

	
	param_tab.push_back(this);
	value=val;
	df_value=val;
	
	type=param_type; 
	uuid = this->generate_my_uuid();
	descriptor = this->generate_my_descriptor(name, info, type);
	

}
Parameter::Parameter()
{}


string Parameter::generate_my_uuid(){
	string copy(PARAM_SERVICE_UUID);
	return copy.replace(36-PARAM_DGCNT,PARAM_DGCNT,to_string_fixed(param_tab.size(),PARAM_DGCNT));
}
void Parameter::param_write(){
	characteristic->writeValue(value);
}
void Parameter::param_read(){
	characteristic->readValue(value);
}
void Parameter::param_to_service(){
	characteristic = new BLEIntCharacteristic (uuid.c_str(), BLERead | BLEWriteWithoutResponse | BLENotify); //affectation operator was not defined
	ble_descriptor = new BLEDescriptor("2901", descriptor.c_str());
	
	paramService->addCharacteristic(*characteristic);
	characteristic->addDescriptor(*ble_descriptor);
}

string Parameter::generate_my_descriptor(string name, string info, PARAM_TYPE type){
	return name  + "||" + std::to_string((int)type) + "||" + info;
}
int16_t Parameter::get_value(){
	return value;
}
const vector <Parameter *>& Parameter::get_param_tab_addr(){
	return param_tab;
}
string Parameter::get_uuid(){
	return uuid;
}
string Parameter::get_descriptor(){
	return descriptor;
}
BLEService& Parameter::get_paramService(){
	return *paramService;
}
BLEIntCharacteristic& Parameter::get_BLEcharacteristic(){
	return *characteristic;
}
void Parameter::param_print(){
	const char* msg1 = (string("uuid: "+ uuid + " & descriptor: " + descriptor+ "& value: " + std::to_string(value))).c_str();
	Serial.println(msg1);
}

// functions
void advertise_paramService(){
	Parameter dummy;
	BLE.setAdvertisedService(dummy.get_paramService());
}
void add_paramService(){
	Parameter dummy;
	BLE.addService(dummy.get_paramService());
}
void all_param_setup(){
	Parameter dummy;
	for(auto const& param_addr: dummy.get_param_tab_addr()){
		param_addr->param_to_service();
	}
	for(auto const& param_addr: dummy.get_param_tab_addr()){
		param_addr->param_write();
	}
}
void all_param_print(){
	Parameter dummy;
	for(auto const& param_addr: dummy.get_param_tab_addr()){
		const char* msg1 = (string("uuid: "+ param_addr->get_uuid() + " descriptor: " + param_addr->get_descriptor())).c_str();
		Serial.println(msg1);
		delete msg1;
	} 
}
void all_param_update(){
	Parameter dummy;
	for(auto const& param_addr: dummy.get_param_tab_addr()){
		if((param_addr->get_BLEcharacteristic()).written()){
			param_addr->param_read();
			if(DEBUG_ON) param_addr->param_print();
		}
	} 
}

