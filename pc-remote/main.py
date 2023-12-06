from threading import Thread
import asyncio, os, uuid

import tkinter as tk
from tkinter import ttk, END, messagebox
from tkinter.filedialog import askopenfilename, asksaveasfilename
from tkinter.font import Font

from utils import*
from constants import*
from custom_logger import*

######################################## GUI #########################################


class GUI:
    def __init__(self):
        self.mainWindow = tk.Tk()
        
        self.ble_thread = BLE_link()
        self.ble_thread.daemon = True
        
        self.ble_thread.start()
        self.widget_tab = []
        self.mode_buttons = []
        self.custom_buttons = []       

        self._init_mainWindow()


        
#----------INITIALIZATION METHODS--------------------------------------------------#
        
    def _init_mainWindow(self):
        style = 'light'

        self.mainWindow.option_add("*tearOff", False)
        self.mainWindow.tk.call('source', 'Theme/Forest-ttk-theme-master/forest-' + style + '.tcl')
        ttk.Style().theme_use('forest-' + style)
            
        self.title_font = Font(
            family = 'Calibri',
            size = 14,
            weight = 'bold',
            slant = 'roman',
            underline = 0,
            overstrike = 0,
        )

        self.title_foreground = '#444777444'

        self.mainWindow.title("FAAV UI")
        self.mainWindow.geometry("700x650")
        self.mainWindow.resizable(0,0)
        
        self.container_frame = ttk.PanedWindow(self.mainWindow)
        self.container_frame.pack(fill=tk.BOTH, expand=True)
        
        self.init_frames()
        self.init_menubar()
        self.init_helpPopup()
        
        self.mainWindow.protocol('WM_DELETE_WINDOW', self.close_app)
        self.mainWindow.mainloop()
        
    def init_frames(self):
        self.buttonsFrame = ttk.Frame(self.container_frame)
        
        for i in range(2):
            self.buttonsFrame.columnconfigure(i,weight=1)
        
        self.init_connexionManager()
        self.init_paramFrame()
        self.init_modeButtons()
        self.init_customButtons()
        self.init_statusFrame()
        
        self.pack_discoverMenu()


    def init_menubar(self):
        self.menubar = tk.Menu(self.mainWindow)
        self.fileMenu = tk.Menu(self.menubar)
        self.measureMenu = tk.Menu(self.menubar)
        self.helpMenu = tk.Menu(self.menubar)

        self.fileMenu.add_command(label="Preferences", command=self.do_nothing) #_load_config
        self.fileMenu.add_separator()
        self.fileMenu.add_command(label="Quit", command=self.close_app)

        # Measure menu
        self.measureMenu.add_command(label="Save as ..", command=self.do_nothing)#_save_measure_as
        self.measureMenu.add_command(label="Save ..", command=self.do_nothing)#_save_measure


        # Create menu bar
        self.menubar.add_cascade(label="File", menu=self.fileMenu)
        self.menubar.add_cascade(label="Measurement", menu=self.measureMenu)
        self.menubar.add_cascade(label="Help", menu=self.helpMenu)

        self.mainWindow.config(menu=self.menubar)

    def close_app(self):
        self.mainWindow.destroy()

    def init_connexionManager(self):    
        self.connexionFrame = ttk.LabelFrame(self.container_frame, text="Connexion Manager",height=350)
        self.connexionFrame.pack_propagate(0)   
        
        self.connectLabel = ttk.Label(self.connexionFrame, text="Connexion with BLE Peripheral", anchor="center")
        self.connectLabel.pack(fill=tk.BOTH)

        self.discoverButton = ttk.Button(self.connexionFrame, text="Discover", cursor="hand2", style='Accent.TButton', command=self.BLE_discover)#_discover
        self.discoverButton.pack(fill = tk.BOTH)

        self.connectButton = ttk.Button(self.connexionFrame, text="Connect", style='Accent.TButton', cursor="hand2", command=self.BLE_connect)#_connect
        self.connectButton.pack(fill=tk.BOTH,side=tk.BOTTOM)
        
        self.connectScroll = ttk.Scrollbar(self.connexionFrame)
        self.connectScroll.pack(side=tk.RIGHT, fill = tk.BOTH)
        
        self.connectList = tk.Listbox(self.connexionFrame, yscrollcommand = self.connectScroll.set,height=200)
        self.connectList.pack(fill = tk.BOTH)
        self.connectScroll.config(command=self.connectList.yview)
    def init_helpPopup(self):
        self.helpWindow = tk.Toplevel()
        self.helpWindow.withdraw() # Hide
        self.helpWindow.title("Help")
        self.helpWindow.geometry("300x200")
        self.helpWindow.resizable(0,0)

        self.helpLabel = ttk.Label(self.helpWindow, text=f"Write me an email: \ncristian.r.fortuna@gmail.com", 
                                   anchor="center").pack()       
    def init_modeButtons(self):
        self.mode_frame = ttk.LabelFrame(self.buttonsFrame, text="Mode", style='TLabelframe')
        self.mode_frame.grid(row=0,column=0,padx=10,pady=5,sticky="w")

        i=0
        for mode in Modes:
            mode_name=mode.name.replace("_"," ")      
            mode_button = ttk.Button(self.mode_frame, text=mode_name, 
                                     command=lambda m=mode: self.set_mode(m),width=9)
            mode_button.pack(side=tk.LEFT, fill=tk.BOTH, padx=5, pady=5)
            self.mode_buttons.append(mode_button)
        self.update_modeButtons()
    def init_customButtons(self):
        self.customBttFrame = ttk.LabelFrame(self.buttonsFrame, text="Custom Buttons", style='TLabelframe')
        self.customBttFrame.grid(row=0,column=1,padx=10,pady=5,sticky="e")
        
        self.sendFlashButton = ttk.Button(self.customBttFrame, text="Send Flash", 
                                     command= self.sendFlashCallback,width=12)
        self.sendFlashButton.grid(column=0,row=0,padx=5,pady=5)
        
        self.eraseButton = ttk.Button(self.customBttFrame, text="Erase Flash", 
                                     command= self.eraseFlashCallback,width=12)
        self.eraseButton.grid(column=1,row=0,padx=5,pady=5)
        
        self.realtime_button = ttk.Button(self.customBttFrame, text="Show Sensors", 
                                     command= self.realtimeSensorsCallback,width=12)
        self.realtime_button.grid(column=2,row=0,padx=5,pady=5)
        

        self.custom_buttons.extend([self.sendFlashButton, self.realtime_button,self.eraseButton])
    def init_statusFrame(self):  
        self.statusFrame=ttk.LabelFrame(self.container_frame, text="Status", style='TLabelframe')
        self.console = ConsoleUi(self.statusFrame) 
        
        self.connectedDevice_label = ttk.Label(self.statusFrame, text=f"Connected to: {self.ble_thread.connectedDevice}")
        self.disconnectButton = ttk.Button(self.statusFrame, text="Disconnect", 
                                command=self.disconnectBtt_callback,width=18,style="Accent.TButton")   
    
    def pack_discoverMenu(self):
        self.connectButton.configure(text="Connect", command=self.BLE_connect)
        self.connexionFrame.pack(side=tk.TOP, fill=tk.X,padx=10)
        self.statusFrame.pack(side=tk.TOP, fill=tk.X, padx=10,pady=5)
    
    def pack_connectionMenu(self):  

        self.buttonsFrame.pack(side=tk.TOP, fill=tk.X, expand=True)
        self.pack_parameterSetup()
    
        match self.ble_thread.mode:
            case Modes.RC_Mode:
                for btt in self.custom_buttons:
                    btt.configure(state='disabled')
                self.paramUpdate_button.configure(state='disabled')
            case Modes.Config:  
                for btt in self.custom_buttons:
                    btt.configure(state='normal')
                self.paramUpdate_button.configure(state='normal')
            case Modes.Sequence: 
                for btt in self.custom_buttons:
                    btt.configure(state='normal')
                self.paramUpdate_button.configure(state='disabled')
            case _:
                mode=Modes.Config
             
        self.update_modeButtons()
        self.resetCustomButtons()
        
        self.connectedDevice_label["text"]=f"Connected to: {self.ble_thread.connectedDevice}"
        self.pack_disconnectButton()
        self.statusFrame.pack(side=tk.TOP, fill=tk.X, padx=10,pady=5)
        
    def hide_discoverMenu(self):   
        self.connexionFrame.pack_forget()
        self.connectList.delete(0, tk.END)
        self.statusFrame.pack_forget()
    def hide_connectionMenu(self):
        self.buttonsFrame.pack_forget()
        
        self.parameterFrame.pack_forget()
        
        self.connectedDevice_label.grid_forget()
        self.disconnectButton.grid_forget()
        self.statusFrame.pack_forget()
    def resetCustomButtons(self):
        self.realtime_button["text"] = "Show Sensors"    
    def BLE_discover(self):
        self.ble_thread.detectedDevices = None 
        self.discoverButton.configure(text="Wait...", command=self.do_nothing)
        self.connectList.delete(0, tk.END)
        self.mainWindow.update()
        self.ble_thread.discover()

        # Maintain tkinter thread alive while waiting for detected devices
        while self.ble_thread.detectedDevices is None:
            self.mainWindow.update()

        print("Device detected")
        
        peripheral = self.ble_thread.detectedDevices
        for p in peripheral:
            if p.name is not None:
                self.connectList.insert(tk.END, str(p.name))

        self.discoverButton.configure(text="Discover", command=self.BLE_discover)
        self.mainWindow.update()
    def BLE_connect(self):
        # Connect to the selected peripheral
        self.connectButton.configure(text="Wait...", command=self.do_nothing)
        self.mainWindow.update()
        self.device2Connect = self.connectList.get(tk.ACTIVE)
        self.ble_thread.connect(deviceName=str(self.device2Connect))

        while self.ble_thread.connectedDevice is None:
            self.mainWindow.update()


        if self.ble_thread.connectedDevice == "Fail":
            logger.warning("Connection to " + str(self.device2Connect) + " failed!")
            self.ble_thread.connectedDevice = None
            self.connectButton.configure(text="Connect", command=self.BLE_connect)
            
        else:
            self.hide_discoverMenu()
            self.pack_connectionMenu()

        self.mainWindow.deiconify()
        self.mainWindow.update()
    def BLE_disconnect(self):
        self.ble_thread.disconnect()      

    def do_nothing(self):
        pass

    def init_paramFrame(self):
        self.parameterFrame = ttk.LabelFrame(self.container_frame, text="Parameters", height=230)        
        self.parameterFrame.pack_propagate(0)
        
        self.param_canvas=tk.Canvas(self.parameterFrame)
        self.param_canvas.pack(fill=tk.BOTH, expand=True,side=tk.LEFT)
        
        self.scrollableFrame=ttk.Frame(self.param_canvas)
        
            
        self.param_v_scrollbar = ttk.Scrollbar(self.parameterFrame, orient=tk.VERTICAL, command=self.param_canvas.yview)
        self.param_v_scrollbar.pack(side=tk.RIGHT, fill=tk.Y,padx=5)
        
        self.scrollableFrame.bind("<Configure>", lambda event: self.param_canvas.configure(scrollregion=self.param_canvas.bbox("all")))
        self.param_canvas.create_window((0, 0), window=self.scrollableFrame, anchor="nw")
        self.param_canvas.configure(yscrollcommand=self.param_v_scrollbar.set)

    def pack_parameterSetup(self):
        nb_params = len(self.ble_thread.param_tab)

        nb_rows=nb_params//4+1
        for i in range(nb_rows):
            max_columns= (nb_params%4 if i==nb_rows-1 else 4) if nb_rows!=1 else nb_params
            for j in range(max_columns):
                self.ble_thread.param_tab[4*i+j].setup_widget(self.scrollableFrame)
                self.ble_thread.param_tab[4*i+j].grid_widget(i,j)
            
        self.paramUpdate_button = ttk.Button(self.scrollableFrame, text="Update Parameters", 
                                          command=self.ble_thread.paramUpdate,width=16)
        self.paramUpdate_button.grid(column=3, row = (nb_rows)*2, sticky="es",padx=10,pady=5)
        
        for row in range(2*(nb_rows)+1):
            self.scrollableFrame.rowconfigure(row, weight= 1,pad=5)
        for i in range(4):
            self.scrollableFrame.columnconfigure(i,uniform="param_col")
        self.parameterFrame.pack(side=tk.TOP, fill=tk.X,padx=10)

    def pack_disconnectButton(self):
        self.disconnectButton["text"]="Disconnect"
        self.disconnectButton.config(state="normal")
        self.connectedDevice_label.grid(column=0,row=1,padx=10,pady=5,sticky="w")
        self.disconnectButton.grid(column=1,row=1,padx=10,pady=5,sticky="e")
        
    def disconnectBtt_callback(self):
        self.disconnectButton["text"]="Disconnecting"
        self.disconnectButton.config(state="disabled")
        self.BLE_disconnect()
        
        self.hide_connectionMenu()
        self.pack_discoverMenu()
        
        self.widget_tab = []
        
        self.mainWindow.update()
        
        
#----------MOD SELECT SCROLL--------------------------------------------------#

        
    def set_mode(self,mode):
        if(self.ble_thread.isConnected):
            match mode:
                case Modes.RC_Mode:
                    for btt in self.custom_buttons:
                        btt.configure(state='disabled')
                    self.paramUpdate_button.configure(state='disabled')
                case Modes.Config:  
                    for btt in self.custom_buttons:
                        btt.configure(state='normal')
                    self.paramUpdate_button.configure(state='normal')
                case Modes.Sequence: 
                    for btt in self.custom_buttons:
                        btt.configure(state='normal')
                    self.paramUpdate_button.configure(state='disabled')
                case _:
                    mode=Modes.Config
            self.ble_thread.new_mode = mode
            self.update_modeButtons()
        else:
            logger.warning("Central not connected to peripheral!")
            
    def update_modeButtons(self):
        for i in range(len(self.mode_buttons)):
            if self.ble_thread.new_mode.name == self.mode_buttons[i]["text"].replace(" ","_"):
                self.mode_buttons[i].configure(style='Accent.TButton')
            else:
                self.mode_buttons[i].configure(style='TButton')

    def realtimeSensorsCallback(self):
        if(self.ble_thread.isConnected):
            current_text = self.realtime_button["text"]
            if current_text == "Show Sensors":
                self.ble_thread.user_command = Commands.sensorDisplay
                self.realtime_button["text"] = "Hide Sensors"
                logger.info("The logger will start displaying sensor values!")
            else:
                self.ble_thread.user_command = Commands.sensorDisplay
                self.realtime_button["text"] = "Show Sensors"
                logger.info("The logger will stop displaying sensor values!")
        else:
            logger.warning("Central not connected to peripheral!")
    def sendFlashCallback(self):
        if(self.ble_thread.isConnected):
            self.ble_thread.user_command = Commands.sendFlash
            logger.debug("Flash transfer started!")
        else:
            logger.warning("Central not connected to peripheral!")

    def eraseFlashCallback(self):
        if(self.ble_thread.isConnected):
            self.ble_thread.user_command = Commands.eraseFlash
            logger.warning("Flash erased!")
        else:
            logger.warning("Central not connected to peripheral!")
 
if __name__ == '__main__':
    root = GUI()
