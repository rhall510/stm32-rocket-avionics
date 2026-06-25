import sys
import os
import serial
import serial.tools.list_ports
import threading
import customtkinter as ctk
from datetime import datetime
from tkinter import filedialog
import json
from collections import defaultdict
import statistics

# --- Matplotlib Integration ---
import matplotlib
matplotlib.use("TkAgg")
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
import matplotlib.pyplot as plt
import matplotlib.dates as mdates
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
        self.data_868 = {'time': [], 'seq': [], 'rssi': [], 'all_times': [], 'is_missed': [], 'rolling_loss': []}
        self.data_24g = {'time': [], 'seq': [], 'rssi': [], 'snr': [], 'all_times': [], 'is_missed': [], 'rolling_loss': []}
        
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
        self.plot_btn = ctk.CTkButton(tools_frame, text="Live Plots", width=90, command=self.toggle_live_plots, fg_color="#1f538d", hover_color="#14375d")
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

    # [Settings, Ports, Connection handling unchanged]
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

    # --- Live Telemetry Parser Hook ---
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
                
            if self.telemetry_buffer[2] != 0x03:
                self.telemetry_buffer = self.telemetry_buffer[2:]
                continue
                
            payload_len = self.telemetry_buffer[3]
            if len(self.telemetry_buffer) < 4 + payload_len:
                break # Wait for the rest of the payload
                
            payload = self.telemetry_buffer[4 : 4 + payload_len]
            self.telemetry_buffer = self.telemetry_buffer[4 + payload_len:] # Advance buffer
            
            if len(payload) == 0:
                continue
                
            source_byte = payload[0]
            is_timeout = (source_byte & 0x80) != 0
            is_24g = (source_byte & 0x01) != 0
            
            time_obj = datetime.now()
            target_data = self.data_24g if is_24g else self.data_868
            
            # Record attempt for PER calculation
            target_data['all_times'].append(time_obj)
            target_data['is_missed'].append(1 if is_timeout else 0)
            
            window = target_data['is_missed'][-10:]
            target_data['rolling_loss'].append((sum(window) / len(window)) * 100.0)
            
            if not is_timeout and len(payload) >= 7:
                seq = int.from_bytes(payload[2:6], byteorder='big')
                rssi = payload[6] - 256 if payload[6] > 127 else payload[6]
                
                target_data['time'].append(time_obj)
                target_data['seq'].append(seq)
                target_data['rssi'].append(rssi)
                
                if is_24g and len(payload) >= 8:
                    snr = payload[7] - 256 if payload[7] > 127 else payload[7]
                    target_data['snr'].append(snr)

    # --- Plotting Window Handler ---
    def toggle_live_plots(self):
        if self.plot_window is None or not self.plot_window.winfo_exists():
            self.plot_window = ctk.CTkToplevel(self.root)
            self.plot_window.title("Live RF Telemetry")
            self.plot_window.geometry("1100x850") # Slightly taller for the control bar
            
            # --- NEW: Control Panel ---
            ctrl_frame = ctk.CTkFrame(self.plot_window, fg_color="transparent")
            ctrl_frame.pack(side=ctk.TOP, fill=ctk.X, padx=10, pady=5)
            
            ctk.CTkLabel(ctrl_frame, text="Data Point Limit:").pack(side=ctk.LEFT, padx=(0, 5))
            
            # Use a StringVar to track the limit
            self.plot_limit_var = ctk.StringVar(value="500")
            self.limit_entry = ctk.CTkEntry(ctrl_frame, textvariable=self.plot_limit_var, width=80)
            self.limit_entry.pack(side=ctk.LEFT)
            # ---------------------------

            self.fig, self.axes = plt.subplots(2, 2, figsize=(11, 8))
            self.fig.suptitle('Live packet test telemetry', fontsize=14, weight='bold')
            
            self.canvas = FigureCanvasTkAgg(self.fig, master=self.plot_window)
            self.canvas.get_tk_widget().pack(fill=ctk.BOTH, expand=True)
            
            self.update_plots()
        else:
            self.plot_window.focus()

    def update_plots(self):
        # Stop looping if the window was closed
        if self.plot_window is None or not self.plot_window.winfo_exists():
            return 
            
        ax1, ax2 = self.axes[0, 0], self.axes[0, 1]
        ax3, ax4 = self.axes[1, 0], self.axes[1, 1]
        
        ax1.clear(); ax2.clear(); ax3.clear(); ax4.clear()
        
        try:
            val = int(self.plot_limit_var.get())
            limit = -abs(val) if val > 0 else -500
        except ValueError:
            limit = -500 # Fallback if user types non-numbers
        
        time_fmt = mdates.DateFormatter('%H:%M:%S')

        # 1. RSSI over Time
        if self.data_868['time']:
            ax1.plot(self.data_868['time'][limit:], self.data_868['rssi'][limit:], linestyle='-', color='#1f77b4', alpha=0.8, label='868MHz')
        if self.data_24g['time']:
            ax1.plot(self.data_24g['time'][limit:], self.data_24g['rssi'][limit:], linestyle='-', color='#2ca02c', alpha=0.8, label='2.4GHz')
        
        ax1.set_title('RSSI over time')
        ax1.set_ylabel('RSSI (dBm)')
        ax1.grid(True, linestyle='--', alpha=0.3)
        ax1.legend()
        ax1.xaxis.set_major_formatter(time_fmt)

        # 2. SNR over Time
        if self.data_24g['time']:
            ax2.plot(self.data_24g['time'][limit:], self.data_24g['snr'][limit:], linestyle='-', color='#d62728', alpha=0.8)
        ax2.set_title('2.4GHz SNR over time')
        ax2.set_ylabel('SNR (dB)')
        ax2.grid(True, linestyle='--', alpha=0.3)
        ax2.xaxis.set_major_formatter(time_fmt)

        # 3. RSSI vs SNR (Mean + Error Band)
        if self.data_24g['snr']:
            snr_groups = defaultdict(list)
            for snr_val, rssi_val in zip(self.data_24g['snr'], self.data_24g['rssi']):
                snr_groups[snr_val].append(rssi_val)
                
            sorted_snrs = sorted(snr_groups.keys())
            mean_rssi, std_rssi = [], []
            
            for snr_val in sorted_snrs:
                rssi_list = snr_groups[snr_val]
                mean_rssi.append(statistics.mean(rssi_list))
                std_rssi.append(statistics.stdev(rssi_list) if len(rssi_list) > 1 else 0.0)

            upper_bound = [m + s for m, s in zip(mean_rssi, std_rssi)]
            lower_bound = [m - s for m, s in zip(mean_rssi, std_rssi)]

            ax3.plot(sorted_snrs, mean_rssi, color='#9467bd', linewidth=2, label='Mean RSSI')
            ax3.fill_between(sorted_snrs, lower_bound, upper_bound, color='#9467bd', alpha=0.3, label='±1 Std Dev')
            
        ax3.set_title('2.4GHz RSSI vs SNR')
        ax3.set_xlabel('SNR (dB)')
        ax3.set_ylabel('RSSI (dBm)')
        ax3.grid(True, linestyle='--', alpha=0.3)
        ax3.legend()

        # 4. Rolling Loss
        if self.data_868['all_times']:
            ax4.plot(self.data_868['all_times'][limit:], self.data_868['rolling_loss'][limit:], linestyle='-', color='#1f77b4', label='868MHz (10-pkt avg)')
        if self.data_24g['all_times']:
            ax4.plot(self.data_24g['all_times'][limit:], self.data_24g['rolling_loss'][limit:], linestyle='-', color='#2ca02c', label='2.4GHz (10-pkt avg)')
            
        ax4.set_title('Rolling packet error rate')
        ax4.set_ylabel('Loss (%)')
        ax4.set_ylim(-5, 105)
        ax4.grid(True, linestyle='--', alpha=0.3)
        ax4.legend()
        ax4.xaxis.set_major_formatter(time_fmt)

        # Formatting tweaks
        for ax in [ax1, ax2, ax4]:
            plt.setp(ax.xaxis.get_majorticklabels(), rotation=45, ha='right')

        self.fig.tight_layout()
        self.canvas.draw()
        
        # Schedule the next frame update (1000ms = 1 sec)
        self.root.after(1000, self.update_plots)

    def handle_incoming_data(self, raw_bytes):
        input_mode = self.input_mode_combo.get()
        if input_mode == "HEX":
            chunk_str = raw_bytes.hex(' ').upper() + "\n"
        else:
            chunk_str = raw_bytes.decode('utf-8', errors='replace')
            
        self.append_to_log(chunk_str, "rx")
        
        # --- NEW: Forward raw bytes to the background telemetry parser ---
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