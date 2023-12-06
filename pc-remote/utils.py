from threading import Thread, Event
import asyncio, uuid
import numpy as np
import sys,os,csv

os.environ['PYGAME_HIDE_SUPPORT_PROMPT'] = 'hide'

from datetime import *
import keyboard
from pysticks import get_controller

from bleak import BleakClient,BleakError
from bleak import BleakScanner
from bleak.backends.characteristic import BleakGATTCharacteristic

from constants import *
from custom_logger import *

import tkinter as tk
from tkinter import ttk, END, messagebox


PARAM_DG_CNT = 4
MAX_COUNTER = 20
TYPES = {0 : "ONE_VAL", 1: "BOOL"}

DEFAULT_PATH = os.path.dirname(os.path.abspath(__file__))
SAVE_FOLDER =DEFAULT_PATH + "/sensor_data"


class Parameter():
    def __init__(self, b_value:bytearray, b_descriptor: bytearray, uuid: str):
        self.name=""
        self.type=0
        self.tags=[]
        self.val=0
        self.uuid=uuid
        
        try:
            self.decode_descriptor(b_descriptor)
        except Exception as err:
            print(err)
            print("Parameter initialized with default values!")
            self.name="Unidentified"
            self.type="ONE_VAL"
            self.tags.append("no units")
        
        try:
            self.decode_val(b_value)
        except Exception as err:
            print(err)
            print("Value initiliazed with 0!")
            self.val=0
            
    def decode_descriptor(self, b_descriptor):
        descriptor = b_descriptor.decode('utf-8')
        self.name, str_type, info = descriptor.split("||")
        self.type=int(str_type)
        if(not self.type in TYPES): 
            print("Type Inconsistency")
        
        self.tags=info.split("#")
        
    def decode_val(self, b_value):
        self.val=int.from_bytes(b_value,"little")
    def print_info(self):
        print(f"Name: {self.name} | Value: {self.val} | Type: {self.type}| Tags: {self.tags}")
    def setup_widget(self, master):
        if (self.type == ""):
            print("Parameter not properly setup. Cant setup widget!") 
            return
        match TYPES[self.type]:
            case "ONE_VAL":
                self.tk_val = tk.IntVar() 
                self.tk_val.set(self.val)
                self.label = ttk.Label(master, text=self.name)
                self.widget = ttk.Entry(master, textvariable=self.tk_val,width=15)
            case "BOOL":
                self.val = bool(self.val)
                self.label = ttk.Label(master,text=self.name)
                self.widget = ttk.Button(master, text=("Turn Off" if self.val else "Turn On"), 
                                                command= self.boolButton_callback,width=15)
            case _:
                print("Not proper type")
    def grid_widget(self, line, clmn):
        self.label.grid(column=clmn, row=line*2)
        self.widget.grid(column=clmn, row=line*2+1)
    def get_widget_val(self):
        match TYPES[self.type]:
            case "ONE_VAL":
                return self.tk_val.get() 
            case "BOOL":
                return (True if self.widget["text"]=='Turn Off' else False)
            case _:
                print("Not proper type")
    def boolButton_callback(self):
        current_text = self.widget["text"]
        if current_text == "Turn On":
            self.widget["text"] = "Turn Off"
        else:
            self.widget["text"] = "Turn On"
    def val_changeDetected(self):
        return (self.val!=self.get_widget_val())    
    def update_paramValue(self):
        match TYPES[self.type]:
            case "ONE_VAL":
                self.val = self.tk_val.get()
            case "BOOL":
                self.val = (True if self.widget["text"]=='Turn Off' else False)
            case _:
                print("Not proper type")


class Sensor():
    def __init__(self, name:str, connect_time: datetime) -> None:
        self.path = SAVE_FOLDER +'\\' + name + "@" + connect_time.strftime("%d_%m_%Y %H_%M_%S") +".csv"
        self.name = name
        self.valuesList = []
        self.timestampsList =[]
        self.column_names = ["time", "data value"]
        
    def writeToCsv(self):
        with open(self.path, 'w', newline='') as file:
            writer = csv.writer(file)
            writer.writerow(['Time', 'Measurement'])
            writer.writerows(zip(self.timestampsList, self.valuesList))
    

class BLE_link(Thread):
    def __init__(self):
        super().__init__()
        

        self.ask2Connect = False
        self.ask2Discover = False
        self.ask2Disconnect = False
        self.device2Connect = None
        self.connectedDevice = None
        self.detectedDevices = None
        self.isConnected = False
        
        self.ask2Update=False
        self.user_command=None
        
        self.showRealtimeSensors=False
        self.sensorDataCounter = 0 
        self.sensorDataNumber = None

        asyncio.ensure_future(self.discoverLoop())
        asyncio.ensure_future(self.connectLoop())
        asyncio.ensure_future(self.user_input(50))
        
        self.param_tab=[]
        self.sensor_tab=[]
        self.mode = Modes.RC_Mode
        self.new_mode = Modes.RC_Mode
        
        self.connect_time=None
        
        if(not os.path.exists(SAVE_FOLDER)): 
            os.makedirs(SAVE_FOLDER)
              
        self.loop = asyncio.get_event_loop()
    
    def run(self):
        self.loop.run_forever()
    def reset(self):
        self.ask2Connect = False
        self.ask2Discover = False
        self.ask2Disconnect = False
        self.device2Connect = None
        self.connectedDevice = None
        self.detectedDevices = None
        self.isConnected = False
        
        self.ask2Update=False
        self.showRealtimeSensors=False
        
        self.param_tab=[]
        self.sensor_tab=[]
        self.mode = Modes.RC_Mode
        self.new_mode = Modes.RC_Mode
        
        self.connect_time=None
    def paramUpdate(self):
        if(self.isConnected):
            self.ask2Update=True
        else:
            logger.warning("Central not connected to peripheral!")

    async def wait2Discover(self):
        while not(self.ask2Discover):
            await asyncio.sleep(0.5)


    async def wait2Connect(self):
        while not(self.ask2Connect):
            await asyncio.sleep(0.5)
        self.ask2Connect = False
########################################################################
#### Setup     
    async def discoverLoop(self):
        while True:
            # Wait for discover request
            await self.wait2Discover()
            print("START DISCOVERING")
            self.detectedDevices = await BleakScanner.discover()
            self.ask2Discover = False
                              
    async def connectLoop(self):
        while True:
            await self.wait2Connect()
            if self.device2Connect is None:
                print("CHOOSE A DEVICE TO CONNECT")
            else:
                for d in self.detectedDevices:
                    if d.name == self.device2Connect:
                        device = d
                        address = d.address
                
                counter = 0         
                while(True):
                    print(device.name)
                    if(counter<=MAX_COUNTER and not self.ask2Disconnect):
                        if(counter==0):
                            logger.debug(f"Trying to connect to {device.name}")
                            try:
                                if (await self.mantainConnect(device,True)): 
                                    break
                                else: counter+=1
                            except BleakError as e:
                                self.connectedDevice = "Fail"
                                print(f"Error: {e}")
                        else:
                            connect_task = asyncio.create_task(BleakScanner.find_device_by_address(address))
                            try:
                                device = await asyncio.wait_for(connect_task, timeout=1)
                            except:
                                logger.warning(f"Central tried to connect {counter} {'time' if counter==1 else 'times'}!")
                                counter+=1
                                continue
                            
                            try:
                                if (device is None): 
                                    counter+=1
                                elif (await self.mantainConnect(device,False)):
                                    counter = 0 
                                    break
                                else: 
                                    counter = 0

                            except:
                                logger.warning(f"Central tried to connect {counter} {'time' if counter==1 else 'times'}!")
                                counter+=1
                    else:
                        logger.error(f"Central has failed to connect to {device.name}")
                        self.isConnected = False
                        counter = 0
                        self.device2Connect = "Fail"
                        spamCounter = 0
                        while(not self.ask2Disconnect):
                            spamCounter+=1
                            if(spamCounter==10):
                                spamCounter = 0
                                logger.error(f'Please click disconnect! The device is disconnected!')
                            await asyncio.sleep(0.5)  
                        self.ask2Disconnect = False  
                        break
                    await asyncio.sleep(1)
                        
    async def mantainConnect(self, device, isFirstConnect):
        async with BleakClient(
        device) as client:
            self.connect_time = datetime.now()
            self.isConnected = True

            if(isFirstConnect):
                logger.debug(f"Connection successful to {device.name}!")
                await self.print_client_info(client)
                await self.connection_setup(client)
                logger.info(f"""{device.name} has the following sensors : {', '.join([sensor.name for sensor in self.sensor_tab])}""")
            else:
                logger.debug(f"Successfully reconnected!")

            self.connectedDevice = device.name  
            await client.start_notify(SENSOR_CHAR_UUID, self.sensorCallback)

            while (not self.ask2Disconnect):
                await self.communicate(client)
                if(not client.is_connected): break
                else: await asyncio.sleep(0.1)

            if(client.is_connected):
                await client.stop_notify(SENSOR_CHAR_UUID)    
                await client.disconnect() 
                logger.debug(f"Central has been disconnected from peripheral!")  
                self.ask2Disconnect = False
                self.isConnected = False
                return True
            else:
                logger.error(f"Central lost connection to {self.connectedDevice}!") 
                self.isConnected = False
                return False
                                    

########################################################################
#### BLE Callbacks       

    def sensorCallback(self, sender, data: bytearray):
        if(self.showRealtimeSensors):
            logger.info("Sensors read: ")
            for id in range(len(self.sensor_tab)):
                value = int.from_bytes(data[2*id:2*id+2],"big")
                logger.info(f"{self.sensor_tab[id].name} reads {value}")
        else:
            if(self.sensorDataNumber == None):
                self.sensorDataNumber = int.from_bytes(data[0:2],"big")
                logger.debug(f"MCU will send {self.sensorDataNumber} data points for each sensor!")
            else:
                self.sensorDataCounter+=1
                timestamp = int.from_bytes(data[0:4],"big")
                timeString = (self.connect_time + timedelta(milliseconds=timestamp)).strftime("%H:%M:%S")
                logger.debug(f"Data recorded at: {timeString}")
                for id in range(len(self.sensor_tab)):
                    value = int.from_bytes(data[2*id+4:2*id+2+4],"big")
                    self.sensor_tab[id].timestampsList.append(timeString)
                    self.sensor_tab[id].valuesList.append(value)
                    logger.info(f"{self.sensor_tab[id].name} reads {value}")
                if(self.sensorDataCounter==self.sensorDataNumber):
                    self.sensorDataCounter = 0
                    self.sensorDataNumber = None
                    logger.debug(f"MCU is done with sending data!")
                    for sensor in self.sensor_tab:
                        sensor.writeToCsv()
                        sensor.valuesList = []
                        sensor.timestampsList =[]
                            
    async def connection_setup(self, client):
        for service in client.services:
            print(service.uuid)
            if(service.uuid == MISC_SERVICE_UUID):
                await self.misc_setup(service,client)
            elif(service.uuid == PARAM_SERVICE_UUID):
                await self.param_setup(service,client)
            elif(service.uuid == SENSOR_SERVICE_UUID):
                await self.sensor_setup(service,client)
                pass
    async def misc_setup(self,service, client):
        for char in service.characteristics:
            print(char.uuid)
            if (char.uuid==MODE_UUID):
                byte_val = await client.read_gatt_char(char.uuid)
                print(int.from_bytes(byte_val,"little"))
                self.mode = Modes(int.from_bytes(byte_val,"little"))
                self.new_mode  = Modes(int.from_bytes(byte_val,"little"))
    async def param_setup(self, service, client):
        for char in service.characteristics:
            if "read" in char.properties:
                try:
                    byte_val = await client.read_gatt_char(char.uuid)
                except Exception as err:
                    print(err)
            else:
                print("Badly defined characteristic")
            for descriptor in char.descriptors:
                if(descriptor.uuid[4:8]!="2901"): 
                    continue
                try:
                    byte_desc = await client.read_gatt_descriptor(descriptor.handle)
                except Exception as err:
                    print(err)
            self.param_tab.append(Parameter(byte_val,byte_desc,char.uuid))
    async def sensor_setup(self, service, client):
        for char in service.characteristics:
            for descriptor in char.descriptors:
                if(descriptor.uuid[4:8]!="2901"): 
                    continue
                try:
                    byte_desc = await client.read_gatt_descriptor(descriptor.handle)
                except Exception as err:
                    print(err)
                descriptor = byte_desc.decode('utf-8')
                sensor_name_list = descriptor.split("#")
                for name in sensor_name_list:
                    self.sensor_tab.append(Sensor(name, self.connect_time))
        

    async def communicate(self,client):
        try:
            if(self.new_mode != self.mode):
                if(self.new_mode.value == 2): 
                    logger.info("Mode set to data callback")
                    
                if(self.new_mode.value == 1):
                    logger.info("Mode set to neutral")
                
                if(self.new_mode.value == 0):
                    logger.info("Mode set to RC pass-through")

                # Update mode memory
                self.mode = self.new_mode 
                await client.write_gatt_char(MODE_UUID, bytearray([self.new_mode.value]))
            
            if self.mode.value == 0:
                #print("Mode switched to RC passthrough")
                #await client.write_gatt_char(RPYT_UUID, bytearray())
                return
            
            
            if(self.user_command != None):
                match self.user_command:
                    case Commands.sensorDisplay:
                        self.showRealtimeSensors = not self.showRealtimeSensors
                        if(self.showRealtimeSensors):
                            logger.debug("Started recording realtime values!")
                        else:
                            logger.debug("Stopped recording realtime values!")
                    case Commands.sendFlash:
                        pass
                    case Commands.eraseFlash:
                        pass

                await client.write_gatt_char(COMMAND_UUID, bytearray([self.user_command.value]))         
                self.user_command = None 

            if self.mode.value == 1:
                if(self.ask2Update):
                    self.ask2Update=False
                    for param in self.param_tab:
                        if(param.val_changeDetected()):
                            param.update_paramValue()
                            logger.info(f"Parameter {param.name} succesfully updated to {param.val}!")
                            await client.write_gatt_char(param.uuid, param.val.to_bytes(2, byteorder='big')) 
            
        except Exception as exc:
            logger.warning("The device is not connected right now!")
            raise RuntimeError("Something bad happened") from exc                

    async def user_input(self,freq):
        while True:
        # Initialize the controller
            try:
                con = get_controller()
            except:
                await asyncio.sleep(2)
                logger.debug("No remote detected. Use keyboard!")
                while True:  # Loop to capture keys continuously
                    await asyncio.sleep(1/freq)
                    keyboard.add_hotkey('space', lambda: print('space was pressed'))

            else:
                await asyncio.sleep(2)
                logger.debug("Taranis detected!")
                con.update()
                if(int(round(con.joystick.get_axis(5))) == 0 and int(round(con.joystick.get_axis(4))) == 0):
                    while True:
                        await asyncio.sleep(1/freq)
                        con.update()
                        global RC_RPYT, RC_mode, RC_sequence
                        RC_RPYT = [con.getRoll(), con.getPitch(), con.getYaw(), con.getThrottle()]
                        RC_RPYT = [int(round((val+1) * 100)) for val in RC_RPYT]  # Normalize the values from -1~1 to 0~180
                        RC_mode = int(round(con.joystick.get_axis(5))) + 1
                        RC_sequence = int(round(con.joystick.get_axis(4))) + 1 
                else:
                    print("Mode and sequence switches not on 0")
                    await asyncio.sleep(2)
    def print_all_param(self):
        for param in self.param_tab:
           param.print_info() 
                                                                                                               

    def connect(self, deviceName=None):
        self.device2Connect = deviceName
        self.ask2Connect = True


    def discover(self):
        self.ask2Discover = True

    def disconnect(self):
        self.reset()
        self.connectedDevice = None

        self.ask2Disconnect = True
    
    
    async def print_client_info(self,client): 
        print("#--------------------- Client Info ---------------------#")
        for service in client.services:
            print("[Service]: ", service)
            for char in service.characteristics:
                if "read" in char.properties:
                    try:
                        value = await client.read_gatt_char(char.uuid)
                        print(" [Characteristic] UUID:", char," | Propreties: "
                            ",".join(char.properties),
                            " | Value:", value,
                        )
                    except Exception as e:
                        print(
                            ("  [Characteristic] Error: ",
                            char,
                            ",".join(char.properties),
                            e,)
                        )

                else:
                    print(
                        "  [Characteristic]", char," | Propreties: ", ",".join(char.properties)
                    )

                for descriptor in char.descriptors:
                    try:
                        value = await client.read_gatt_descriptor(descriptor.handle)
                        print("    [Descriptor] UUID: ", descriptor," | Value:", value)
                    except Exception as e:
                        print("    [Descriptor] UUID: ", descriptor, e)
        


