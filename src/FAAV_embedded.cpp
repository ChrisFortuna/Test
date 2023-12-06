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

//////////////////////////////////////////////////////////////////////////////////////////////
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
Task tSensorsControl(TASK_MILLISECOND*5000, TASK_FOREVER, &tSensorsCallback);
Task tFlashSend(TASK_SECOND*2, TASK_FOREVER, &tFlashSendCallback);

//////////////////////////////////////////////////////////////////////////////////////////////
//MCU default settings:

int t1Time = 0;
int seq_preset_pwm = 800;
int seq_preset_delay = 1;
int seq_preset_timeout = 1;

bool realtimeSensors = false;
int  currentMode = RC_MODE ;
bool flashTransferActive = false;
uint16_t infoFlash[2] ={0};
uint32_t dataSent = 0;

int initial_delay = 0;
char builtinLED_value = 'R';


vector<Parameter *> Parameter::param_tab;
BLEService* Parameter::paramService;

vector<Sensor *> Sensor::sensor_tab={};
BLEService* Sensor::sensorService;

//////////////////////////////////////////////////////////////////////////////////////////////
//Parameter list:
Parameter frequency ( " Frequency ( Hz ) " , 60 , ONE_VAL , " range (20 ,100) " ) ;
Parameter ledBlink ( " Blink Led ? " , 0 , BOOL );
Parameter blinkFrequency ( " Blink Frequency ( Hz ) " , 200 , ONE_VAL , " range (20 ,100) " ) ;
Parameter param1 ( " Water Threshold " , 20 , ONE_VAL , " range (20 ,100) " ) ;
Parameter param2 ( " Power Threshold ? " , 0 , BOOL );
Parameter param3 ( " Current Threshold  " , 200 , ONE_VAL , " range (20 ,100) " ) ;

//Sensor list:
Sensor waterSensor("Water Sensor",2);
Sensor voltageSensor("Loose Sensor",4);
Sensor currentSensor("Current Sensor",5);

//////////////////////////////////////////////////////////////////////////////////////////////
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


void modeChangeHandler(int oldMode){

	if(currentMode == RC_MODE){
		tSensorsControl.disable();
	}
	if(currentMode==PARAM_MODE){
		if(realtimeSensors) tSensorsControl.enable();
		else tSensorsControl.disable();
	}
	if(currentMode == SEQUENCE){
		tSensorsControl.enable();
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
		if(DEBUG_ON){
			Serial.print("Flap ON with PWM us: ");
			Serial.println(seq_preset_pwm);
		}
	}
	else{
		ESC.writeMicroseconds(800);	
		if(DEBUG_ON) Serial.println("Flap OFF");
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





// the loop function runs over and over again forever
void loop() {
	rpyt_struct rpyt;
	// listen for BLE centrals to connect:
	BLEDevice central = BLE.central();

	// if a central is connected to peripheral:
	if (central) {
		if(DEBUG_ON){
			Serial.print("Connected to central: ");
			Serial.println(central.address());
		}
		resetTimeReference();
		// // while the central is still connected to peripheral:
		while (central.connected()) {
			runner.execute();
		// 	// Check if a value has been written on the characteristic "valChar"     
			if (cBLE_mode.written()) {
				int oldMode = currentMode;
				currentMode = cBLE_mode.value();
				if(DEBUG_ON){
					Serial.print("Mode changed to: ");
					Serial.println(currentMode);
				}
				modeChangeHandler(oldMode);
			}
			runner.execute();

			switch (currentMode){
			case RC_MODE:
				if (cBLE_RPYT.written()) {
			 		uint8_t data[4];
			 		cBLE_RPYT.readValue(data, 4);
					if(DEBUG_ON){
						Serial.print("Roll Pitch Yaw Thrust ");
						for(int i= 0; i<4; i++){
							Serial.print(data[i]);
							Serial.print(" ");
						} 
						Serial.println(" ");
					}
			 		rpyt.roll = data[0]; 
					rpyt.pitch = data[1]; 
					rpyt.yaw = data[2]; 
					rpyt.thrust = data[3];

					manual_flight_controller(rpyt);
				}
				break;
			case PARAM_MODE:
				all_param_update();
				break;
			case SEQUENCE:
				//write the sequence here
				break;
			}
			delay(10); // Can be increased
		}

	} 
	resetRobot();

	runner.execute();
	delay(20); // Can be increased
}
//////////////////////////////////////////////////////////////////////////////////////////////
// Setup Functions:
// the setup function runs once when you press reset or power the board
void setup() {
	
	delay(4000);
	if(DEBUG_ON) Serial.begin(9600);
	// begin initialization
	if (!BLE.begin()) {

		if(DEBUG_ON) Serial.println("STARTING BLE FAILED!");
		while (1);
	}	

	resetRobot();

	pixels.begin(); //Neopixel initialization

	initTaskScheduler(); //TaskScheduler initialization
	initBle(); //BLE initialization

	initGPIOs();
	initFlash();
}
void initTaskScheduler(){
	runner.init();
	runner.addTask(t1);
	runner.addTask(t1off);
	runner.addTask(tFlappingOFF);
	runner.addTask(tFlappingON);
	runner.addTask(tSensorsControl);
	runner.addTask(tFlashSend);

	delay(500);
	t1.enable();
	t1off.enableDelayed(100);
	// tPowerSampler.enable();
}

void initGPIOs(){
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

void initBle(void){
	// set advertised local name and service UUID:
	BLE.setTimeout(1);
	BLE.setSupervisionTimeout(100); // The actual timeout will be (value * 10 ms).
	BLE.setLocalName(DEVICE_NAME);
	
	advertise_paramService();
	advertise_sensorService();

	setupMiscService();
	all_param_setup();
	allSensorSetup();

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
	allSensorPrint();
	delay(100);
	if(DEBUG_ON) Serial.println("Advertising started!");
}
void setupMiscService(){
	BLE.setAdvertisedService(MISC_SERVICE);
	miscService.addCharacteristic(cBLE_mode);
	miscService.addCharacteristic(cBLE_RPYT);
	miscService.addCharacteristic(cBLE_Command);

	cBLE_Command.setEventHandler(BLEWritten,BleCommandCallback);

	BLE.addService(miscService);
}


void resetRobot(){
	currentMode=RC_MODE;
	cBLE_mode.writeValue(RC_MODE);
	realtimeSensors = false;
	flashTransferActive = false;

	t1Time = 0;
	seq_preset_pwm = 800;
	seq_preset_delay = 1;
	seq_preset_timeout = 1;

	infoFlash[0] = 0;
	infoFlash[1] = 0; 
	dataSent = 0;

	initial_delay = 0;
	builtinLED_value = 'R';
}




////////////////////////////////////////////////////////////////////////////////////
// BLE Callbacks:
void BleCommandCallback(BLEDevice central, BLECharacteristic characteristic) {
	if(DEBUG_ON) Serial.println("Command has been received!");

	byte flagRegister(cBLE_Command.value());
	if (flagRegister & 1) {
		if(flashTransferActive) {
			if(DEBUG_ON) Serial.println("Flash transfer already active");
		}
		else {
			readFlashInfo(infoFlash);
			tFlashSend.enable();
		}
	} 
	if (flagRegister & 2) {
		realtimeSensors =!realtimeSensors;
		if(realtimeSensors){
			if(DEBUG_ON) Serial.println("Sensors will show in realtime!");
			tSensorsControl.enable();
		}
		else{
			if(currentMode==SEQUENCE) tSensorsControl.enable();
			else tSensorsControl.disable();
			if(DEBUG_ON) Serial.println("Sensors will stop showing in real time!");
		}
	}
	if (flagRegister & 4) {
//Flash is not erased in the real sense. The pointer is simply reset!
		if(DEBUG_ON) Serial.println("Flash erased!");
		resetFlash();
	}
}

void onBLEConnected(BLEDevice central) {
	if(DEBUG_ON) {
		Serial.print("Connected event, central: ");
		Serial.println(central.address());
	}
	builtinLED_value = 'G';
}

void onBLEDisconnected(BLEDevice central) {
	if(DEBUG_ON) {
		Serial.print("Disconnected event, central: ");
		Serial.println(central.address());
	}
	
	tSensorsControl.disable();
	tFlashSend.disable();
	builtinLED_value = 'R';
}
//////////////////////////////////////////////////////////////////////////////////////////////
// Task Callbacks:
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
		allSensorRecordViaBle();
	}else{
		if(flashTransferActive){
			if(DEBUG_ON) Serial.println("Flash Transfer Active!");
		}else{
			allSensorRecordViaFlash();
		}
	}
}
void tFlashSendCallback(){
	if(flashTransferActive){
		if(infoFlash[0] == dataSent) {
			infoFlash[0] = 0;
			infoFlash[1] = 0;
			dataSent = 0;
			flashTransferActive = false;

			tFlashSend.disable();
		}
		else {
			uint8_t tabSize(getSensorNb() + 2); //+2 because timestamp
			
			uint16_t buff[tabSize] ={0};
			uint8_t tempBuff[tabSize*2]={0};

			readOneLine(dataSent,buff);
		// Despite it being taboo, tabSize>=2 so acceptable code bellow..
		//big-endian encoding
			tempBuff[0] = (byte)((buff[0]>>8) & 0x00FF);

			tempBuff[1] = (byte)(buff[0] & 0x00FF);

			tempBuff[2] = (byte)((buff[1]>>8) & 0x00FF);
			tempBuff[3] = (byte)(buff[1] & 0x00FF);

			if(DEBUG_ON){
				Serial.println("#########");
				Serial.print("Millis: ");
				Serial.print(tempBuff[0]);
				Serial.print('_');
				Serial.print(tempBuff[1]);
				Serial.print('_');
				Serial.print(tempBuff[2]);
				Serial.print('_');
				Serial.print(tempBuff[3]);
				Serial.println("");
			}

			for(uint8_t i(2); i<tabSize;++i){
				tempBuff[2*i] = (byte)((buff[i]>>8) & 0x00FF);
				tempBuff[2*i+1] = (byte)(buff[i] & 0x00FF);
				if(DEBUG_ON){
					Serial.print(tempBuff[2*i]);
					Serial.print('_');
					Serial.print(tempBuff[2*i+1]);
					if(i!=tabSize-1) Serial.print(",");
					else Serial.println("");
				}
			}

			sendBufferViaBLE(tempBuff, tabSize*2);
			++dataSent;
		}
	}
	else{
		flashTransferActive = true;
		
		uint8_t tempBuff[4] ={0};
		//big endian encoding, first is the MSB
		tempBuff[0] = (byte)((infoFlash[0]>>8) & 0x00FF);
		tempBuff[1] = (byte) (infoFlash[0] & 0x00FF);

		tempBuff[2] = (byte)((infoFlash[1]>>8) & 0x00FF);
		tempBuff[3] = (byte) (infoFlash[1] & 0x00FF);

		sendBufferViaBLE(tempBuff, 4);
	}
}




