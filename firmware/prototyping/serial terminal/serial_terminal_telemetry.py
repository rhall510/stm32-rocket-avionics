import sys
import os
import serial
import serial.tools.list_ports
import threading
import customtkinter as ctk
from datetime import datetime
from tkinter import filedialog
import json
import struct
import math
import numpy as np

# --- Matplotlib Integration ---
import matplotlib
matplotlib.use("TkAgg")
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d.art3d import Poly3DCollection
# Use a dark theme to match CustomTkinter
plt.style.use("dark_background")

# Set the overall theme for CustomTkinter
ctk.set_appearance_mode("Dark")
ctk.set_default_color_theme("blue")

# --- PYINSTALLER PATH FIX ---
if getattr(sys, 'frozen', False):
    application_path = os.path.dirname(sys.executable)
else:
    application_path = os.path.dirname(os.path.abspath(__file__))

CONFIG_FILE = os.path.join(application_path, "terminal_settings.json")
# ----------------------------

# --- Helper Math Functions ---
def euler_to_matrix(yaw_deg, pitch_deg, roll_deg):
    """Converts Yaw (Z), Pitch (Y), and Roll (X) in degrees to a rotation matrix"""
    y = math.radians(yaw_deg)
    p = math.radians(pitch_deg)
    r = math.radians(roll_deg)
    
    cy, sy = math.cos(y), math.sin(y)
    cp, sp = math.cos(p), math.sin(p)
    cr, sr = math.cos(r), math.sin(r)
    
    R_yaw = np.array([
        [cy, -sy, 0],
        [sy,  cy, 0],
        [ 0,   0, 1]
    ])
    R_pitch = np.array([
        [ cp, 0, sp],
        [  0, 1,  0],
        [-sp, 0, cp]
    ])
    R_roll = np.array([
        [1,  0,   0],
        [0, cr, -sr],
        [0, sr,  cr]
    ])
    
    return R_yaw @ R_pitch @ R_roll

def body_to_plot_frame(verts, R):
    # Rotate vertices from Body to Nav frame (NED)
    v_nav = (R @ verts.T).T
    # Map Math/Nav frame (North, East, Down) to Plot frame (East, North, Up)
    v_plot = np.column_stack((v_nav[:, 1], v_nav[:, 0], -v_nav[:, 2]))
    return v_plot

class ModernSerialTerminal:
    def __init__(self, root):
        self.root = root
        self.root.title("Bidirectional Serial Terminal")
        self.root.geometry("1000x550") 
        
        self.ser = None
        self.is_running = False
        self.is_new_line = True
        
        # Live Recording Variables
        self.is_recording = False
        self.log_file_handle = None
        self.record_buffer = ""
        
        # --- Live Telemetry Variables ---
        self.telemetry_buffer = bytearray()
        self.plot_window = None
        
        # Storage for the decoded floats
        self.avionics_data = {
            'x': [], 'y': [], 'z': [],
            'vx': [], 'vy': [], 'vz': [],
            'yaw': [], 'pitch': [], 'roll': []
        }
        
        # Basic 3D aircraft model definitions
        self.base_verts = np.array([
            [ 2.0,  0.0,  0.0],  # 0: Nose
            [-1.0, -1.5,  0.0],  # 1: Left wing tip
            [-1.0,  1.5,  0.0],  # 2: Right wing tip
            [-1.0,  0.0, -0.5],  # 3: Top tail fin (-Z is up in NED)
            [-1.0,  0.0,  0.2]   # 4: Bottom fuselage
        ])
        self.faces_idx = [
            [0, 1, 3], # Top-left wing-to-fin (TOP)
            [0, 2, 3], # Top-right wing-to-fin (TOP)
            [0, 1, 4], # Bottom-left (BOTTOM)
            [0, 2, 4], # Bottom-right (BOTTOM)
            [1, 2, 3], # Back plate top (TOP)
            [1, 2, 4]  # Back plate bottom (BOTTOM)
        ]
        
        self.setup_ui()
        
        self.refresh_ports()
        self.load_settings()

    def setup_ui(self):
        # --- Top: Connection Bar ---
        conn_frame = ctk.CTkFrame(self.root)
        conn_frame.pack(side=ctk.TOP, fill=ctk.X, padx=10, pady=(10, 5))

        self.port_combo = ctk.CTkComboBox(conn_frame, width=220, values=["Scanning..."])
        self.port_combo.pack(side=ctk.LEFT, padx=(10, 5), pady=10)

        self.baud_combo = ctk.CTkComboBox(conn_frame, width=100, values=["9600", "19200", "38400", "57600", "115200", "230400", "460800", "921600"])
        self.baud_combo.set("115200")
        self.baud_combo.pack(side=ctk.LEFT, padx=5, pady=10)

        self.refresh_btn = ctk.CTkButton(conn_frame, text="Refresh", width=70, command=self.refresh_ports)
        self.refresh_btn.pack(side=ctk.LEFT, padx=5, pady=10)

        self.connect_btn = ctk.CTkButton(conn_frame, text="Connect", width=100, command=self.toggle_connection, fg_color="green", hover_color="darkgreen")
        self.connect_btn.pack(side=ctk.RIGHT, padx=10, pady=10)

        # --- Middle-Top: Tools Bar ---
        tools_frame = ctk.CTkFrame(self.root, fg_color="transparent")
        tools_frame.pack(side=ctk.TOP, fill=ctk.X, padx=10, pady=(0, 5))

        self.clear_btn = ctk.CTkButton(tools_frame, text="Clear", width=60, command=self.clear_log, fg_color="gray50", hover_color="gray30")
        self.clear_btn.pack(side=ctk.LEFT)

        self.save_btn = ctk.CTkButton(tools_frame, text="Save Snapshot", width=100, command=self.save_log, fg_color="gray50", hover_color="gray30")
        self.save_btn.pack(side=ctk.LEFT, padx=10)

        self.record_btn = ctk.CTkButton(tools_frame, text="Start Recording", width=110, command=self.toggle_recording, fg_color="gray50", hover_color="gray30")
        self.record_btn.pack(side=ctk.LEFT)
        
        # --- NEW: Live Plot Toggle Button ---
        self.plot_btn = ctk.CTkButton(tools_frame, text="Avionics 3D", width=90, command=self.toggle_live_plots, fg_color="#1f538d", hover_color="#14375d")
        self.plot_btn.pack(side=ctk.LEFT, padx=10)

        self.filter_entry = ctk.CTkEntry(tools_frame, width=150, placeholder_text="Filter prefix (e.g. DATA:)")
        self.filter_entry.pack(side=ctk.LEFT, padx=10)

        self.autoscroll_var = ctk.BooleanVar(value=True)
        self.autoscroll_check = ctk.CTkCheckBox(tools_frame, text="Auto-Scroll", variable=self.autoscroll_var)
        self.autoscroll_check.pack(side=ctk.RIGHT, padx=5)

        # --- Bottom: Input Area ---
        input_frame = ctk.CTkFrame(self.root)
        input_frame.pack(side=ctk.BOTTOM, fill=ctk.X, padx=10, pady=(0, 10))

        self.input_mode_combo = ctk.CTkOptionMenu(input_frame, width=80, values=["ASCII", "HEX"])
        self.input_mode_combo.set("ASCII")
        self.input_mode_combo.pack(side=ctk.LEFT, padx=(5, 0), pady=10)

        self.entry_box = ctk.CTkEntry(input_frame, font=("Consolas", 14), placeholder_text="Type command here...")
        self.entry_box.pack(side=ctk.LEFT, fill=ctk.X, expand=True, padx=(10, 5), pady=10)
        self.entry_box.bind("<Return>", self.send_message)

        self.line_end_combo = ctk.CTkOptionMenu(input_frame, width=110, values=["CRLF (\\r\\n)", "LF (\\n)", "CR (\\r)", "None"])
        self.line_end_combo.set("CRLF (\\r\\n)")
        self.line_end_combo.pack(side=ctk.LEFT, padx=5, pady=10)

        self.send_btn = ctk.CTkButton(input_frame, text="Send", width=80, command=self.send_message)
        self.send_btn.pack(side=ctk.RIGHT, padx=(5, 10), pady=10)

        # --- Middle: Incoming Data Window ---
        self.log_area = ctk.CTkTextbox(self.root, font=("Consolas", 13), fg_color="black")
        self.log_area.pack(side=ctk.TOP, fill=ctk.BOTH, expand=True, padx=10, pady=(0, 10))
        
        self.log_area.tag_config("rx", foreground="#00FF00")        
        self.log_area.tag_config("tx", foreground="#00BFFF")        
        self.log_area.tag_config("info", foreground="#FFD700")      
        self.log_area.tag_config("error", foreground="#FF4500")     
        self.log_area.tag_config("timestamp", foreground="#888888") 
        
        self.log_area.configure(state="disabled")

    def load_settings(self):
        if os.path.exists(CONFIG_FILE):
            try:
                with open(CONFIG_FILE, "r") as f:
                    settings = json.load(f)
                if "baud_rate" in settings: self.baud_combo.set(settings["baud_rate"])
                if "line_ending" in settings: self.line_end_combo.set(settings["line_ending"])
                if "autoscroll" in settings: self.autoscroll_var.set(settings["autoscroll"])
                if "filter_prefix" in settings: self.filter_entry.insert(0, settings["filter_prefix"])
                if "input_mode" in settings: self.input_mode_combo.set(settings["input_mode"])
                if "port" in settings:
                    saved_port = settings["port"]
                    available_ports = self.port_combo.cget("values")
                    for p in available_ports:
                        if p.startswith(saved_port):
                            self.port_combo.set(p)
                            break
            except Exception as e:
                self.append_to_log(f"[WARNING] Could not load settings: {e}\n", tag="error")

    def save_settings(self):
        current_port_str = self.port_combo.get()
        port_only = current_port_str.split(" - ")[0] if " - " in current_port_str else ""
        settings = {
            "port": port_only,
            "baud_rate": self.baud_combo.get(),
            "line_ending": self.line_end_combo.get(),
            "autoscroll": self.autoscroll_var.get(),
            "filter_prefix": self.filter_entry.get(),
            "input_mode": self.input_mode_combo.get()
        }
        try:
            with open(CONFIG_FILE, "w") as f:
                json.dump(settings, f, indent=4)
        except Exception as e:
            print(f"Failed to save settings: {e}")

    def refresh_ports(self):
        ports = serial.tools.list_ports.comports()
        port_list = [f"{p.device} - {p.description}" for p in ports]
        if not port_list:
            port_list = ["No ports found"]
        self.port_combo.configure(values=port_list)
        if self.port_combo.get() == "Scanning...":
            self.port_combo.set(port_list[0])

    def toggle_connection(self):
        if self.is_running: self.disconnect()
        else: self.connect_to_serial()

    def connect_to_serial(self):
        selection = self.port_combo.get()
        if selection == "No ports found" or "Scanning" in selection:
            self.append_to_log("[ERROR] Select a valid COM port\n", tag="error")
            return
        port_name = selection.split(" - ")[0]
        try:
            baud_rate = int(self.baud_combo.get())
        except ValueError:
            self.append_to_log("[ERROR] Invalid baud rate.\n", tag="error")
            return

        try:
            self.ser = serial.Serial(port_name, baud_rate, timeout=1, write_timeout=1)
            self.ser.dtr = True
            self.ser.rts = True
            
            self.is_running = True
            self.append_to_log(f"\n--- Connected to {port_name} at {baud_rate} baud ---\n", tag="info")
            
            self.connect_btn.configure(text="Disconnect", fg_color="red", hover_color="darkred")
            self.port_combo.configure(state="disabled")
            self.baud_combo.configure(state="disabled")
            self.refresh_btn.configure(state="disabled")
            
            self.read_thread = threading.Thread(target=self.read_from_port)
            self.read_thread.daemon = True
            self.read_thread.start()
        except Exception as e:
            self.append_to_log(f"\n[CONNECTION ERROR] Could not open {port_name}:\n{e}\n", tag="error")

    def disconnect(self):
        self.is_running = False
        if self.ser and self.ser.is_open:
            self.ser.close()
        if self.is_recording:
            self.toggle_recording()
        self.append_to_log("\n--- Disconnected ---\n", tag="info")
        self.connect_btn.configure(text="Connect", fg_color="green", hover_color="darkgreen")
        self.port_combo.configure(state="normal")
        self.baud_combo.configure(state="normal")
        self.refresh_btn.configure(state="normal")

    def toggle_recording(self):
        if self.is_recording:
            self.is_recording = False
            if self.log_file_handle:
                self.log_file_handle.close()
                self.log_file_handle = None
            self.record_btn.configure(text="Start Recording", fg_color="gray50", hover_color="gray30")
            self.append_to_log("\n--- Live Recording Stopped ---\n", tag="info")
        else:
            file_path = filedialog.asksaveasfilename(
                defaultextension=".csv",
                filetypes=[("CSV/Log Files", "*.csv *.log"), ("All Files", "*.*")],
                title="Select Live Log File Location"
            )
            if file_path:
                try:
                    self.log_file_handle = open(file_path, "a", encoding="utf-8")
                    self.is_recording = True
                    self.record_buffer = "" 
                    self.record_btn.configure(text="Stop Recording", fg_color="red", hover_color="darkred")
                    self.append_to_log(f"\n--- Live Recording to {file_path} ---\n", tag="info")
                except Exception as e:
                    self.append_to_log(f"\n[ERROR: Could not start recording - {e}]\n", tag="error")

    def read_from_port(self):
        while self.is_running:
            try:
                if self.ser.in_waiting > 0:
                    raw_bytes = self.ser.read(self.ser.in_waiting)
                    self.root.after(0, self.handle_incoming_data, raw_bytes)
            except Exception as e:
                if self.is_running:
                    self.root.after(0, self.append_to_log, f"\n[Read Error: {e}]\n", "error")
                    self.root.after(0, self.disconnect)
                break

    # --- Live Avionics Parser Hook ---
    def parse_live_telemetry(self, raw_bytes):
        self.telemetry_buffer.extend(raw_bytes)
        
        while len(self.telemetry_buffer) >= 4:
            sync_idx = self.telemetry_buffer.find(b'\xbe\xeb')
            
            if sync_idx == -1:
                # No complete sync word, keep the last byte in case a split BEEB is arriving
                self.telemetry_buffer = self.telemetry_buffer[-1:] 
                break
                
            if sync_idx > 0:
                self.telemetry_buffer = self.telemetry_buffer[sync_idx:]
                continue
                
            msg_type = self.telemetry_buffer[2]
            payload_len = self.telemetry_buffer[3]
            
            if len(self.telemetry_buffer) < 4 + payload_len:
                break # Wait for the rest of the payload
                
            payload = self.telemetry_buffer[4 : 4 + payload_len]
            self.telemetry_buffer = self.telemetry_buffer[4 + payload_len:] # Advance buffer
            
            # Message Type 0x08 is the Telemetry Data
            # Length 0x24 (36 bytes) -> 9 little endian floats
            if msg_type == 0x08 and payload_len == 36:
                try:
                    data = struct.unpack('<9f', payload)
                    self.avionics_data['x'].append(data[0])
                    self.avionics_data['y'].append(data[1])
                    self.avionics_data['z'].append(data[2])
                    self.avionics_data['vx'].append(data[3])
                    self.avionics_data['vy'].append(data[4])
                    self.avionics_data['vz'].append(data[5])
                    self.avionics_data['yaw'].append(data[6])
                    self.avionics_data['pitch'].append(data[7])
                    self.avionics_data['roll'].append(data[8])
                except struct.error:
                    pass

    # --- Plotting Window Handler ---
    def toggle_live_plots(self):
        if self.plot_window is None or not self.plot_window.winfo_exists():
            self.plot_window = ctk.CTkToplevel(self.root)
            self.plot_window.title("Avionics 3D Telemetry")
            self.plot_window.geometry("1400x700") 
            
            ctrl_frame = ctk.CTkFrame(self.plot_window, fg_color="transparent")
            ctrl_frame.pack(side=ctk.TOP, fill=ctk.X, padx=10, pady=5)
            
            ctk.CTkLabel(ctrl_frame, text="Path Point Limit:").pack(side=ctk.LEFT, padx=(0, 5))
            
            self.plot_limit_var = ctk.StringVar(value="500")
            self.limit_entry = ctk.CTkEntry(ctrl_frame, textvariable=self.plot_limit_var, width=80)
            self.limit_entry.pack(side=ctk.LEFT)

            self.fig = plt.figure(figsize=(14, 7))
            self.fig.suptitle('Live 3D Flight Kinematics', fontsize=16, weight='bold')
            
            self.ax1 = self.fig.add_subplot(121, projection='3d')
            self.ax2 = self.fig.add_subplot(122, projection='3d')
            
            # --- INITIALIZE PLOT ELEMENTS ONCE ---
            self.ax1.set_xlabel('East (X) [meters]')
            self.ax1.set_ylabel('North (Y) [meters]')
            self.ax1.set_zlabel('Altitude (Z) [meters]')
            self.ax1.set_title("Flight Path & Orientation")
            
            self.ax2.set_xlabel('East (X)')
            self.ax2.set_ylabel('North (Y)')
            self.ax2.set_zlabel('Up (Z)')
            self.ax2.set_title("Orientation Only")
            self.ax2.set_xlim(-1, 1)
            self.ax2.set_ylim(-1, 1)
            self.ax2.set_zlim(-1, 1)

            # Trail line
            self.trail_line, = self.ax1.plot([], [], [], color='red', alpha=0.8, linestyle='--', linewidth=2)
            
            # Polygons
            self.plane1 = Poly3DCollection([], facecolors=['teal', 'teal', 'cyan', 'cyan', 'teal', 'cyan'], edgecolors='blue', alpha=0.9)
            self.ax1.add_collection3d(self.plane1)
            
            self.plane2 = Poly3DCollection([], facecolors=['saddlebrown', 'saddlebrown', 'orange', 'orange', 'saddlebrown', 'orange'], edgecolors='maroon', alpha=0.9)
            self.ax2.add_collection3d(self.plane2)
            # -------------------------------------

            self.canvas = FigureCanvasTkAgg(self.fig, master=self.plot_window)
            self.canvas.get_tk_widget().pack(fill=ctk.BOTH, expand=True)
            
            self.update_plots()
        else:
            self.plot_window.focus()

    def update_plots(self):
        if self.plot_window is None or not self.plot_window.winfo_exists():
            return 
            
        if not self.avionics_data['x']:
            self.root.after(100, self.update_plots)
            return

        try:
            val = int(self.plot_limit_var.get())
            limit = -abs(val) if val > 0 else -500
        except ValueError:
            limit = -500 
            
        px_hist = np.array(self.avionics_data['x'][limit:])
        py_hist = np.array(self.avionics_data['y'][limit:])
        pz_hist = np.array(self.avionics_data['z'][limit:])
        
        cur_yaw = self.avionics_data['yaw'][-1]
        cur_pitch = self.avionics_data['pitch'][-1]
        cur_roll = self.avionics_data['roll'][-1]
        
        plot_e = py_hist
        plot_n = px_hist
        plot_u = -pz_hist

        R = euler_to_matrix(cur_yaw, cur_pitch, cur_roll)
        
        # Dynamically scale ax1 limits to the tracking history
        max_range = np.max([plot_e.max()-plot_e.min(), plot_n.max()-plot_n.min(), plot_u.max()-plot_u.min()]) / 2.0
        if max_range < 1.0: 
            max_range = 1.0
            
        mid_e = (plot_e.max() + plot_e.min()) * 0.5
        mid_n = (plot_n.max() + plot_n.min()) * 0.5
        mid_u = (plot_u.max() + plot_u.min()) * 0.5
        
        self.ax1.set_xlim(mid_e - max_range, mid_e + max_range)
        self.ax1.set_ylim(mid_n - max_range, mid_n + max_range)
        self.ax1.set_zlim(mid_u - max_range, mid_u + max_range)
        
        # --- FAST DATA UPDATES INSTEAD OF CLEARING ---
        # Update path trail
        self.trail_line.set_data_3d(plot_e, plot_n, plot_u)
        
        # Update Airplane 1 (Path)
        axis_len = max_range * 0.15 
        verts_scaled = self.base_verts * (axis_len * 0.4) 
        v_plot_1 = body_to_plot_frame(verts_scaled, R) + np.array([plot_e[-1], plot_n[-1], plot_u[-1]])
        faces_1 = [[v_plot_1[idx] for idx in face] for face in self.faces_idx]
        self.plane1.set_verts(faces_1)

        # Update Airplane 2 (Origin)
        verts_unit = self.base_verts * 0.4
        v_plot_2 = body_to_plot_frame(verts_unit, R)
        faces_2 = [[v_plot_2[idx] for idx in face] for face in self.faces_idx]
        self.plane2.set_verts(faces_2)

        self.canvas.draw_idle()
        
        # 50ms interval ~ 20fps
        self.root.after(50, self.update_plots)

    def handle_incoming_data(self, raw_bytes):
        input_mode = self.input_mode_combo.get()
        if input_mode == "HEX":
            chunk_str = raw_bytes.hex(' ').upper() + "\n"
        else:
            chunk_str = raw_bytes.decode('utf-8', errors='replace')
            
        self.append_to_log(chunk_str, "rx")
        
        # Forward raw bytes to the background telemetry parser
        self.parse_live_telemetry(raw_bytes)
        
        if self.is_recording and self.log_file_handle:
            self.record_buffer += chunk_str
            while '\n' in self.record_buffer:
                line, self.record_buffer = self.record_buffer.split('\n', 1)
                clean_line = line.replace('\r', '')
                prefix = self.filter_entry.get().strip()
                if not prefix or clean_line.startswith(prefix):
                    raw_time = datetime.now().strftime("%H:%M:%S.%f")[:-3]
                    try:
                        self.log_file_handle.write(f"[{raw_time}] {clean_line}\n")
                        self.log_file_handle.flush() 
                    except Exception as e:
                        self.append_to_log(f"\n[RECORDING ERROR] {e}\n", tag="error")
                        self.toggle_recording()

    def send_message(self, event=None):
        if not self.ser or not self.ser.is_open:
            self.append_to_log("[WARNING] Not connected to a device\n", tag="error")
            return
            
        user_message = self.entry_box.get()
        if not user_message: return

        input_mode = self.input_mode_combo.get()
        
        if input_mode == "ASCII":
            endings = {"CRLF (\\r\\n)": "\r\n", "LF (\\n)": "\n", "CR (\\r)": "\r", "None": ""}
            ending_str = endings.get(self.line_end_combo.get(), "\r\n")
            data_to_send = (user_message + ending_str).encode('utf-8')
            display_text = f"> {user_message}"
            
        elif input_mode == "HEX":
            clean_hex = user_message.replace("0x", "").replace("0X", "").replace(" ", "").replace(",", "")
            try:
                data_to_send = bytes.fromhex(clean_hex)
                formatted_hex = " ".join(clean_hex[i:i+2] for i in range(0, len(clean_hex), 2)).upper()
                display_text = f"> [HEX] {formatted_hex}"
            except ValueError:
                self.append_to_log("\n[ERROR] Invalid HEX input. Use formats like '0xAABB' or 'AA BB'\n", tag="error")
                return

        try:
            self.ser.write(data_to_send)
            self.ser.flush()
            self.append_to_log(f"{display_text}\n", tag="tx")
            self.entry_box.delete(0, 'end')
            
            if self.is_recording and self.log_file_handle:
                raw_time = datetime.now().strftime("%H:%M:%S.%f")[:-3]
                self.log_file_handle.write(f"[{raw_time}] {display_text}\n")
                self.log_file_handle.flush()
        except serial.SerialTimeoutException:
            self.append_to_log("\n[ERROR: Write timeout]\n", tag="error")
        except Exception as e:
            self.append_to_log(f"\n[Write error: {e}]\n", tag="error")

    def append_to_log(self, text, tag="rx"):
        self.log_area.configure(state="normal")
        text = text.replace('\r', '')
        raw_time = datetime.now().strftime("%H:%M:%S.%f")[:-3]
        timestamp = f"[{raw_time}] "
        
        parts = text.split('\n')
        for i, part in enumerate(parts):
            if self.is_new_line and part:
                self.log_area.insert("end", timestamp, "timestamp")
                self.is_new_line = False
            if part:
                self.log_area.insert("end", part, tag)
            if i < len(parts) - 1:
                self.log_area.insert("end", "\n", tag)
                self.is_new_line = True

        if self.autoscroll_var.get():
            self.log_area.see("end")
        self.log_area.configure(state="disabled")

    def clear_log(self):
        self.log_area.configure(state="normal")
        self.log_area.delete("1.0", "end")
        self.log_area.configure(state="disabled")
        self.is_new_line = True

    def save_log(self):
        file_path = filedialog.asksaveasfilename(
            defaultextension=".txt",
            filetypes=[("Text Files", "*.txt"), ("Log Files", "*.log"), ("All Files", "*.*")],
            title="Save Terminal Snapshot"
        )
        if file_path:
            try:
                log_content = self.log_area.get("1.0", "end-1c")
                with open(file_path, "w", encoding="utf-8") as file:
                    file.write(log_content)
                self.append_to_log(f"\n--- Snapshot saved to {file_path} ---\n", tag="info")
            except Exception as e:
                self.append_to_log(f"\n[ERROR: Could not save file - {e}]\n", tag="error")

    def on_closing(self):
        self.save_settings()
        if self.is_recording and self.log_file_handle:
            self.log_file_handle.close()
        self.is_running = False
        if self.ser and self.ser.is_open:
            self.ser.close()
        self.root.destroy()

if __name__ == '__main__':
    root = ctk.CTk()
    app = ModernSerialTerminal(root)
    root.protocol("WM_DELETE_WINDOW", app.on_closing)
    root.mainloop()