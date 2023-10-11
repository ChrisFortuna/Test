# -*- coding: utf-8 -*-
import asyncio
from bleak import BleakClient
from bleak import BleakError
from pysticks import get_controller
import keyboard
from datetime import datetime
import pandas as pd


RC_RPYT = 0
RC_mode = 0
RC_sequence = 0
RC_sequence_prev = 0
RC_mode_prev = -1
BLEconnected = False

UUID_mode = '13012F03-F8C3-4F4A-A8F4-15CD926DA147'
UUID_RPYT = '13012F03-F8C3-4F4A-A8F4-15CD926DA148'
UUID_Voltage = '13012F03-F8C3-4F4A-A8F4-15CD926DA149'
UUID_Current = '13012F03-F8C3-4F4A-A8F4-15CD926DA150'
UUID_Water = '13012F03-F8C3-4F4A-A8F4-15CD926DA151'
UUID_Time = '13012F03-F8C3-4F4A-A8F4-15CD926DA152'
UUID_ParamSequence = '13012F03-F8C3-4F4A-A8F4-15CD926DA153'
UUID_RunSequence = '13012F03-F8C3-4F4A-A8F4-15CD926DA154'
UUID_ParamValueSequence =  '13012F03-F8C3-4F4A-A8F4-15CD926DA155'

# Initialize lists to store sensor data
# voltage_values = []
# current_values = []
# water_values = []
# time_values = []

SEEEDUINO_A = '63:82:D5:C5:83:29'
SEEEDUINO_B = 'E5:82:3F:F6:97:34'
SEEEDUINO_ADR = SEEEDUINO_B

#################################### Data return ################################

class FAAV_Data:
	def __init__(self):
		self.voltage_values = []
		self.current_values = []
		self.water_values = []
		self.time_values = []

	def add_voltage_to_array(self, data):
		self.voltage_values.append(data)

	def add_current_to_array(self, data):
		self.current_values.append(data)

	def add_water_to_array(self, data):
		self.water_values.append(data)

	def add_time_to_array(self, data):
		self.time_values.append(data)
	
	def buffer_to_csv(self):
		# datetime object containing current date and time
		dt_string = datetime.now().strftime("%Y.%m.%d.%H.%M.%S")
		# dict = {'Voltage': self.voltage_values, 'Current': self.current_values} 
		dfv = pd.DataFrame({'Voltage': self.voltage_values})
		dfi = pd.DataFrame({'Current': self.current_values})
		dfw = pd.DataFrame({'Water': self.water_values})
		dft = pd.DataFrame({'Time': self.time_values})

		df = pd.concat([dft, dfv, dfi, dfw], axis=1) # Concat tolerates different array lengths
		df.drop(df.tail(1).index,inplace=True) # drop last row of END OF FRAME VALUE
		# Save to file
		df.to_csv('sensordata/FAAV_data_' + dt_string + '.csv')
		
	def clear_data(self):
		del self.voltage_values[:]
		del self.current_values[:]
		del self.water_values[:]
		del self.time_values[:]

Sensordata = FAAV_Data()

async def voltage_notification_handler(sender, data: bytearray):
	global Sensordata
	voltage = int.from_bytes(data,'little')
	Sensordata.add_voltage_to_array(voltage)
	# Handle end of file
	if(voltage == 1000000):
		await asyncio.sleep(0.5) # gives some time for transmission of other data to finish
		end_of_transmission_handler()
		return	

def current_notification_handler(sender, data: bytearray):
	data_int = int.from_bytes(data,'little')
	global Sensordata
	Sensordata.add_current_to_array(data_int)

def water_notification_handler(sender, data: bytearray):
	data_int = int.from_bytes(data,'little')
	global Sensordata
	Sensordata.add_water_to_array(data_int)

def time_notification_handler(sender, data: bytearray):
	data_int = int.from_bytes(data,'little')
	print(data_int)
	global Sensordata
	Sensordata.add_time_to_array(data_int)

def end_of_transmission_handler():
	global Sensordata
	Sensordata.buffer_to_csv()
	Sensordata.clear_data()
	print("Data transfer complete")


#################################### handle_connected  ################################

async def handle_connected(client): # yoyoooo
	global RC_mode_prev, RC_mode, RC_sequence_prev, RC_sequence
	val = 0

	# Handle mode change
	if(RC_mode_prev != RC_mode):
		if(RC_mode == 2): # getting to mode 2
			print("Mode changed to data callback")
			await client.start_notify(UUID_Voltage, voltage_notification_handler)
			await client.start_notify(UUID_Current, current_notification_handler)
			await client.start_notify(UUID_Water, water_notification_handler)
			await client.start_notify(UUID_Time, time_notification_handler)
		if(RC_mode_prev == 2): # getting out of mode 2
			await client.stop_notify(UUID_Voltage)
			await client.stop_notify(UUID_Current)
			await client.stop_notify(UUID_Water)
			await client.stop_notify(UUID_Time)

		if(RC_mode == 1):
			print("Mode set to neutral")
		
		if(RC_mode == 0):
			print("Mode changed to RC pass-through")

		# Update mode memory
		RC_mode_prev = RC_mode 
		
		await client.write_gatt_char(UUID_mode, bytearray([RC_mode]))
	
	if RC_mode == 0:
		# print("Mode switched to RC passthrough")
		await client.write_gatt_char(UUID_RPYT, bytearray(RC_RPYT))

	# Handle sequence change
	if RC_mode == 1 and RC_sequence_prev != RC_sequence:
		RC_sequence_prev = RC_sequence
		if(RC_sequence == 0): # getting to sequence setting
			# IS BLOCKING !!
			param = input('Choose:\n y: Start Flapping \n t: flapping timeout (sec) \n d: delay (sec) \
		     \n p: power flapping (1-100)% \n a: angle of symetric folding (deg, 0 is fully extended) \n')
			await client.write_gatt_char(UUID_ParamSequence, (param.encode('utf-8')))
			val = int(input('Value: \n'))
			if(val >=0 and val <= 200):
				await client.write_gatt_char(UUID_ParamValueSequence, bytearray([val]))

		if(RC_sequence == 2): # getting to sequence run
			await client.write_gatt_char(UUID_RunSequence, bytearray([1]))
			val = int.from_bytes(await client.read_gatt_char(UUID_RunSequence), "big")
			if val:
				print("Sequence started!") 

		if(RC_sequence == 1):
			await client.write_gatt_char(UUID_RunSequence, bytearray([0]))
			print("Sequence set to neutral")


#################################### RUN FUNCTION ###################################
def on_disconnect(client):
	global BLEconnected 
	BLEconnected = False
	print("Disconnected callback called!")
	# client.stop_notify(UUID_Voltage)
	# disconnected_event.set()

async def ble():
	global BLEconnected

	while True:
		await asyncio.sleep(0.1)

		if not BLEconnected:
			try:
				address = SEEEDUINO_ADR
				print(f'Trying to connect to BLE device {address} ... ', end="")
				async with BleakClient(address, disconnected_callback=on_disconnect) as client:
					print(f'Connected')
					BLEconnected = True
					while True:
						if not BLEconnected:
							break
						await asyncio.sleep(0.02)
						await handle_connected(client)
			except BleakError as e:
				print("Could not connect, retrying")
				# print(e)
				# await client.disconnect()
		

######################################### PYSTICKS ##################################

async def user_input(freq):
	while True:
	# Initialize the controller
		try:
			con = get_controller()
		except:
			print("No remote")
			while True:  # Loop to capture keys continuously
				await asyncio.sleep(2)
				keyboard.add_hotkey('space', lambda: print('space was pressed'))

		else:
			print("Taranis connected")
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
	
######################################### MAIN ##################################
	   
loop = asyncio.get_event_loop()
asyncio.ensure_future(ble())
asyncio.ensure_future(user_input(50)) # freq in Hz
loop.run_forever()

