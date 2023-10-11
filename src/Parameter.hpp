#ifndef PARAMETER_H
#define PARAMETER_H

#include <string>
#include <vector>

using std::string;
using std::vector;

enum PARAM_TYPE {ONE_VAL,BOOL,BUTTON};

class Parameter{
	public:
	Parameter(string name, int16_t val, PARAM_TYPE type = ONE_VAL, string info = "");
	Parameter();

//constructor methods:
	string generate_my_uuid();
	string generate_my_descriptor(string name, string info, PARAM_TYPE type);

//setup param methods:
	void param_write();
	void param_to_service();
	
//update param methods:	
	void param_read();

//getters
	unsigned get_param_nb();
	BLEIntCharacteristic& get_BLEcharacteristic();
	const vector <Parameter *>& get_param_tab_addr();
	BLEService& get_paramService();
	string get_uuid();
	int16_t get_value();
	string get_descriptor();
	void param_print();

  protected:
    	int16_t value;
	int16_t df_value;
	string uuid;
	string descriptor;
	PARAM_TYPE type;
	
	static vector<Parameter *> param_tab;
      static BLEService* paramService;

	BLEIntCharacteristic* characteristic;
	BLEDescriptor* ble_descriptor;   
};

void add_paramService();
void advertise_paramService();
void all_param_setup();
void all_param_print();
void all_param_update();

#endif
