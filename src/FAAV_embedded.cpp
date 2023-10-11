#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <ArduinoBLE.h>  
#include <Servo.h>
#include <TaskScheduler.h>

#include "FAAV_embedded.hpp"
#include "Macros.hpp"
#include "Parameter.hpp"
#include "Sensor.hpp"
#include "QSPI_flash.hpp"

#include <string>
#include <vector>

using std::string;
using std::vector;

BLEService miscService(MISC_SERVICE); // BLE Service
BLEByteCharacteristic cBLE_mode(MODE_UUID, BLEWriteWithoutResponse | BLERead);
BLEByteCharacteristic cBLE_RPYT(RPYT_UUID, BLEWriteWithoutResponse | BLERead);
BLEByteCharacteristic cBLE_Command(COMMAND_UUID, BLERead | BLEWriteWithoutResponse);

Adafruit_NeoPixel pixels(1, 9, NEO_GRBW);
Servo ESC;  // create servo object to control a servo
Servo wingServo_l;
Servo wingServo_r;

Scheduler runner;
Task t1(TASK_SECOND/2, TASK_FOREVER, &t1Callback); //us/ms, #executions, callback fn
Task t1off(TASK_SECOND/2, TASK_FOREVER, &t1offCallback); 
Task tFlappingOFF(TASK_SECOND, TASK_ONCE, &tFlappingOFFCallback);
Task tFlappingON(TASK_SECOND, TASK_ONCE, &tFlappingONCallback);
Task taskSensors(TASK_MILLISECOND*5000, TASK_FOREVER, &tSensorsCallback);

int t1Time = 0;
int seq_preset_pwm = 800;
int seq_preset_delay = 1;
int seq_preset_timeout = 1;

bool realtimeSensors = false;
int  currentMode = RC_MODE ;

int initial_delay = 0;
char builtinLED_value = 'K';

vector<Parameter *> Parameter::param_tab;
BLEService* Parameter::paramService;

vector<Sensor *> Sensor::sensor_tab={};
BLEService* Sensor::sensorService;

//Parameter list

//Parameter param1("Param1",0,BOOL);
Parameter frequency ( " Frequency ( Hz ) " , 60 , ONE_VAL , " range (20 ,100) " ) ;
Parameter ledBlink ( " Blink Led ? " , 0 , BOOL );
Parameter blinkFrequency ( " Blink Frequency ( Hz ) " , 200 , ONE_VAL , " range (20 ,100) " ) ;
Parameter param1 ( " Water Threshold " , 20 , ONE_VAL , " range (20 ,100) " ) ;
Parameter param2 ( " Power Threshold ? " , 0 , BOOL );
Parameter param3 ( " Current Threshold  " , 200 , ONE_VAL , " range (20 ,100) " ) ;

Sensor waterSensor("Water Sensor",0);
Sensor voltageSensor("Voltage Sensor",1);
Sensor currentSensor("Current Sensor",2);

void manual_flight_controller(struct rpyt_struct rpyt){
	rpyt_struct remapped;

	// roll_corrected = constrain((int)rpyt.roll + OFFSET_ROLL, 0, 200);
	// pitch_corrected = constrain((int)rpyt.pitch + OFFSET_PITCH, 0, 200);

	// if(rpyt.roll > 100){
		
	// }

	remapped.roll = map(rpyt.roll,0,200,0,180);
	remapped.pitch = map(rpyt.pitch,0,200,0,180);
	remapped.thrust = map(rpyt.thrust,0,200,800,2000);
	wingServo_l.write(remapped.roll);
	wingServo_r.write(180-remapped.roll);
	ESC.writeMicroseconds(remapped.thrust);
}

// This is a blocking function !
void bleSendFlash(){
	builtinLED('B');
	flashBleSend();
	// while(!flashBleSend()){
	// 	++counter;
	// 	const char* msg = ("Tried " + std::to_string(counter) + " times!").c_str();
	// 	Serial.println(msg);
	// 	delete msg;
	// 	if(counter == MAX_BAD_FLASH_READS){
	// 		Serial.println("BLE flash sensor data transfer failed!");
	// 		return;
	// 	}
	// }	
}

void modeChangeHandler(int oldMode){
	if(oldMode == SEQUENCE){
		bleSendFlash();
	}

	if(currentMode == RC_MODE){
		taskSensors.disable();
	}
	if(currentMode==PARAM_MODE){
		if(realtimeSensors) taskSensors.enable();
		else taskSensors.disable();
	}
	if(currentMode == SEQUENCE){
		taskSensors.enable();
		builtinLED_value = 'K';
	}

}

void builtinLED(char color){
	switch (color){
	case 'G': digitalWrite(LEDR, HIGH); digitalWrite(LEDG, LOW); digitalWrite(LEDB, HIGH); break;
	case 'R': digitalWrite(LEDR, LOW); digitalWrite(LEDG, HIGH); digitalWrite(LEDB, HIGH); break;
	case 'B': digitalWrite(LEDR, HIGH); digitalWrite(LEDG, HIGH); digitalWrite(LEDB, LOW); break;
	case 'W': digitalWrite(LEDR, LOW); digitalWrite(LEDG, LOW); digitalWrite(LEDB, LOW); break;
	case 'K': digitalWrite(LEDR, HIGH); digitalWrite(LEDG, HIGH); digitalWrite(LEDB, HIGH); break;
	}
	// pixels.setPixelColor(0, pixels.Color(0, 0, 255,0));
    	// pixels.show();   // Send the updated pixel colors to the hardware.
}
void onBLEConnected(BLEDevice central) {
	Serial.print("Connected event, central: ");
	Serial.println(central.address());
	builtinLED_value = 'G';
}

void onBLEDisconnected(BLEDevice central) {
	Serial.print("Disconnected event, central: ");
	Serial.println(central.address());
	
	taskSensors.disable();
	builtinLED_value = 'R';
}


// the setup function runs once when you press reset or power the board
void setup() {
	
	delay(4000);
	Serial.begin(9600);
	// begin initialization
	if (!BLE.begin()) {
		Serial.println("STARTING BLE FAILED!");
		while (1);
	}	

	init_flash();

	init_GPIOs();
	pixels.begin(); //Neopixel initialization

	init_TaskScheduler(); //TaskScheduler initialization
	init_ble(); //BLE initialization
	builtinLED_value = 'R';
}

// the loop function runs over and over again forever
void loop() {
	rpyt_struct rpyt;
	// listen for BLE centrals to connect:
	BLEDevice central = BLE.central();

	// if a central is connected to peripheral:
	if (central) {
		Serial.print("Connected to central: ");
		// print the central's MAC address:
		Serial.println(central.address());
		reset_time_reference();
		// // while the central is still connected to peripheral:
		while (central.connected()) {
			runner.execute();
		// 	// Check if a value has been written on the characteristic "valChar"     
			if (cBLE_mode.written()) {
				int oldMode = currentMode;
				currentMode = cBLE_mode.value();
				Serial.print("Mode changed to: ");
				Serial.println(currentMode);
				modeChangeHandler(oldMode);
			}
			runner.execute();

			switch (currentMode){
			case RC_MODE:
				if (cBLE_RPYT.written()) {
			 		uint8_t data[4];
			 		cBLE_RPYT.readValue(data, 4);
			 		Serial.print("Roll Pitch Yaw Thrust ");
			 		for(int i= 0; i<4; i++){Serial.print(data[i]); Serial.print(" ");} Serial.println(" ");
			 		rpyt.roll = data[0]; rpyt.pitch = data[1]; rpyt.yaw = data[2]; rpyt.thrust = data[3];

					manual_flight_controller(rpyt);
				}
				break;
			case PARAM_MODE:
				all_param_update();
				break;
			case SEQUENCE:
				break;
			}
			delay(10); // Can be increased
		}

	} 
	resetRobot();

	runner.execute();
	delay(20); // Can be increased
}


void init_TaskScheduler(){
	runner.init();
	runner.addTask(t1);
	runner.addTask(t1off);
	runner.addTask(tFlappingOFF);
	runner.addTask(tFlappingON);
	runner.addTask(taskSensors);
	delay(500);
	t1.enable();
	t1off.enableDelayed(100);
	// tPowerSampler.enable();
}

void init_GPIOs(){
	// initialize digital pin LED_BUILTIN as an output.
	pinMode(LED_BUILTIN, OUTPUT);
	ESC.attach(PIN_ESC); // attaches the servo on pin to the servo object
	builtinLED('W');
	ESC.writeMicroseconds(2000);

	delay(3000);
	ESC.writeMicroseconds(1000);
	builtinLED('R');
	delay(2000);

	wingServo_l.attach(PIN_WINGSERVO_L);
	wingServo_r.attach(PIN_WINGSERVO_R);
	pinMode(PIN_ISENSE, INPUT);
	pinMode(PIN_VSENSE, INPUT);
}

void init_ble(void){
	// set advertised local name and service UUID:
	BLE.setTimeout(1);
	BLE.setSupervisionTimeout(100); // The actual timeout will be (value * 10 ms).
	BLE.setLocalName(DEVICE_NAME);
	
	advertise_paramService();
	advertise_sensorService();

	setup_miscService();
	all_param_setup();
	all_sensor_setup();

	// add service

	add_paramService();
	add_sensorService();

	BLE.setEventHandler(BLEConnected, onBLEConnected);
  	BLE.setEventHandler(BLEDisconnected, onBLEDisconnected);

	// set the initial value to 0:
	cBLE_mode.writeValue(0);

	// start advertising
	BLE.advertise();
	
	all_param_print();
	all_sensor_print();
	delay(100);
	Serial.println("#---------- Start Advertising -----------#");
}
void setup_miscService(){
	BLE.setAdvertisedService(MISC_SERVICE);
	miscService.addCharacteristic(cBLE_mode);
	miscService.addCharacteristic(cBLE_RPYT);
	miscService.addCharacteristic(cBLE_Command);

	cBLE_Command.setEventHandler(BLEWritten,cBleCommandCallback);

	BLE.addService(miscService);
}


void resetRobot(){
	currentMode=RC_MODE;
	cBLE_mode.writeValue(RC_MODE);
	realtimeSensors = false;
}
void cBleCommandCallback(BLEDevice central, BLECharacteristic characteristic) {
	Serial.println("Command sent! ");

	byte flag_rg(cBLE_Command.value());
	if (flag_rg & 1) {
		reset_flash(); //it is blocking for now
	} 
	if (flag_rg & 2) {
		realtimeSensors =!realtimeSensors;
		if(realtimeSensors){
			Serial.println("Sensors will show in realtime!");
			if(currentMode==PARAM_MODE) taskSensors.enable();
		}
		else Serial.println("Sensors will stop showing in real time!");
	}
	if (flag_rg & 4) {

	}
}


void wing_sweep(int sweep){
	int offset = 135;
	wingServo_l.write(offset - sweep);
	wingServo_r.write(180-(offset - sweep));
}

void flapping_active(bool active){
	if (active){
		ESC.writeMicroseconds(seq_preset_pwm);
		Serial.print("Flap ON with PWM us: ");
		Serial.println(seq_preset_pwm);
	}
	else{
		ESC.writeMicroseconds(800);	
		Serial.println("Flap OFF");
	}
}

void t1Callback() {
	// Heartbeat ON
	builtinLED(builtinLED_value);
}

void t1offCallback() {
	// Heartbeat OFF
	builtinLED('K');
}

void tFlappingONCallback() {
	flapping_active(true);
	tFlappingOFF.restartDelayed(seq_preset_timeout*1000); // turns flapping ON after delay
}

void tFlappingOFFCallback() {
	flapping_active(false);
}

void tSensorsCallback(){
	if(realtimeSensors){
		all_sensor_ble_send();
	}else{
		all_sensor_record();
	}
}





