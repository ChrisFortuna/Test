from enum import *
    
ONE_VALUE=0
TWO_VALUE=1
COMPLEX=2

PARAM_SERVICE_UUID="889e60ca-aa43-4b44-abdb-3d98ca8d8d87"
SENSOR_SERVICE_UUID="c195183b-53c0-425c-93cd-9f7bdf5027bb"
SENSOR_CHAR_UUID="c195183b-53c0-425c-93cd-9f7bdf5027b0"
MISC_SERVICE_UUID = "13012f00-f8c3-4f4a-a8f4-15cd926da146"

RPYT_UUID= "655c8dfe-5940-11ee-8c99-0242ac120002"
MODE_UUID= "054c764d-70b6-4952-8576-7a04b11b08af"
COMMAND_UUID= "0592b69b-087f-47ba-9112-e37c8ba29281"

class Modes(Enum):
    RC_Mode = 0
    Configuration = 1
    Sequence = 2
    
class Commands(Enum):
    Erase = 0b0000_0001
    Sensor_Display = 0b0000_0010
    
