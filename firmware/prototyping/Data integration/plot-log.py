import sys
import struct
import math
import re
import numpy as np
from datetime import datetime

import matplotlib.pyplot as plt
from matplotlib.widgets import Slider, Button
from mpl_toolkits.mplot3d.art3d import Poly3DCollection
import matplotlib.animation as animation

plt.style.use("dark_background")

# --- Helper Math Functions ---
def euler_to_matrix(yaw_deg, pitch_deg, roll_deg):
    y = math.radians(yaw_deg)
    p = math.radians(pitch_deg)
    r = math.radians(roll_deg)
    
    cy, sy = math.cos(y), math.sin(y)
    cp, sp = math.cos(p), math.sin(p)
    cr, sr = math.cos(r), math.sin(r)
    
    R_yaw = np.array([[cy, -sy, 0], [sy,  cy, 0], [0, 0, 1]])
    R_pitch = np.array([[cp, 0, sp], [0, 1, 0], [-sp, 0, cp]])
    R_roll = np.array([[1, 0, 0], [0, cr, -sr], [0, sr, cr]])
    
    return R_yaw @ R_pitch @ R_roll

def body_to_plot_frame(verts, R):
    v_nav = (R @ verts.T).T
    v_plot = np.column_stack((v_nav[:, 1], v_nav[:, 0], -v_nav[:, 2]))
    return v_plot

# --- Data Parser ---
def load_data(filepath):
    index = 0

    data = {
        'time': [], 'x': [], 'y': [], 'z': [],
        'vx': [], 'vy': [], 'vz': [],
        'roll': [], 'pitch': [], 'yaw': [],
        'temp': [], 'press': []
    }
    first_time = None
    fallback_time = 0.0
    
    try:
        with open(filepath, "r") as f:
            for line in f:
                if "BE EB 08 2C" in line:
                    # 1. Extract the timestamp from the start of the line
                    time_match = re.search(r'\[(.*?)\]', line)
                    base_t_sec = fallback_time
                    if time_match:
                        try:
                            t_str = time_match.group(1)
                            t_obj = datetime.strptime(t_str, "%H:%M:%S.%f")
                            if first_time is None: 
                                first_time = t_obj
                            base_t_sec = (t_obj - first_time).total_seconds()
                        except ValueError:
                            pass
                    
                    # 2. Split the line by the sync word to catch multiple packets
                    # parts[0] will be the timestamp prefix, parts[1:] will be the payloads
                    parts = line.split("BE EB 08 2C ")
                    
                    for i, part in enumerate(parts[1:]):
                        hex_str = part.replace(" ", "")
                        if len(hex_str) >= 88:
                            try:
                                payload = bytes.fromhex(hex_str[:88])
                                vals = struct.unpack('<11f', payload)
                                
                                # Add a tiny 10ms offset to subsequent packets on the same line 
                                # so they don't stack on the exact same timestamp
                                packet_time = base_t_sec + (i * 0.01)
                                
                                data['time'].append(packet_time)
                                data['x'].append(vals[0])
                                data['y'].append(vals[1])
                                data['z'].append(vals[2])
                                data['vx'].append(vals[3])
                                data['vy'].append(vals[4])
                                data['vz'].append(vals[5])
                                data['roll'].append(vals[6])
                                data['pitch'].append(vals[7])
                                data['yaw'].append(vals[8])
                                data['temp'].append(vals[9])
                                data['press'].append(vals[10])
                                
                                fallback_time = packet_time + 0.05

                                index = index + 1

                                with open("splitlog.csv", "a") as splitlogf:
                                    valstring = f"{vals}"[1:-1]
                                    splitlogf.write(f"{index},{valstring}\n")
                            except Exception:
                                pass
    except FileNotFoundError:
        print(f"Error: {filepath} not found.")
        sys.exit()
        
    for k in data: 
        data[k] = np.array(data[k])
    return data

data = load_data("log2.csv")
if len(data['time']) == 0:
    print("No telemetry data found in log.csv!")
    sys.exit()

t_hist = data['time']
plot_n = data['x']
plot_e = data['y']
plot_u = data['z']
roll_hist = data['roll']
pitch_hist = data['pitch']
yaw_hist = data['yaw']
temp_hist = data['temp']
press_hist = data['press']

# --- Setup Figure & Layout ---
fig = plt.figure(figsize=(14, 9))
fig.suptitle('Avionics Telemetry Playback', fontsize=16, weight='bold')

gs = fig.add_gridspec(3, 2, width_ratios=[2, 1], bottom=0.15)
ax1 = fig.add_subplot(gs[:, 0], projection='3d')
ax2 = fig.add_subplot(gs[0, 1], projection='3d')
ax_temp = fig.add_subplot(gs[1, 1])
ax_press = fig.add_subplot(gs[2, 1])

ax1.set_xlabel('East (X) [meters]')
ax1.set_ylabel('North (Y) [meters]')
ax1.set_zlabel('Altitude (Z) [meters]')
ax1.set_title("Flight Path & Orientation")

ax2.set_xlabel('East (X)')
ax2.set_ylabel('North (Y)')
ax2.set_zlabel('Up (Z)')
ax2.set_title("Orientation Only")
ax2.set_xlim(-1, 1)
ax2.set_ylim(-1, 1)
ax2.set_zlim(-1, 1)
ax2.set_xticklabels([])
ax2.set_yticklabels([])
ax2.set_zticklabels([])

ax_temp.set_title("Temperature")
ax_temp.set_ylabel("Temp (°C)")
ax_temp.grid(True, linestyle='--', alpha=0.3)

ax_press.set_title("Pressure")
ax_press.set_ylabel("Pressure (Pa)")
ax_press.set_xlabel("Time (s)")
ax_press.grid(True, linestyle='--', alpha=0.3)

base_verts = np.array([
    [ 2.0,  0.0,  0.0],  # 0: Nose
    [-1.0, -1.5,  0.0],  # 1: Left wing tip
    [-1.0,  1.5,  0.0],  # 2: Right wing tip
    [-1.0,  0.0, -0.5],  # 3: Top tail fin
    [-1.0,  0.0,  0.2]   # 4: Bottom fuselage
])
faces_idx = [[0, 1, 3], [0, 2, 3], [0, 1, 4], [0, 2, 4], [1, 2, 3], [1, 2, 4]]

# --- Initialize Plot Elements ---
trail_line, = ax1.plot([], [], [], color='red', alpha=0.8, linestyle='--', linewidth=2)
plane1 = Poly3DCollection([], facecolors=['teal', 'teal', 'cyan', 'cyan', 'teal', 'cyan'], edgecolors='blue', alpha=0.9)
ax1.add_collection3d(plane1)
plane2 = Poly3DCollection([], facecolors=['saddlebrown', 'saddlebrown', 'orange', 'orange', 'saddlebrown', 'orange'], edgecolors='maroon', alpha=0.9)
ax2.add_collection3d(plane2)
line_temp, = ax_temp.plot([], [], color='tab:red', linewidth=2)
line_press, = ax_press.plot([], [], color='tab:blue', linewidth=2)

# --- Interactivity Controls ---
ax_slider = fig.add_axes([0.15, 0.05, 0.55, 0.03])
time_slider = Slider(
    ax=ax_slider, label='Time Frame', valmin=0, valmax=len(t_hist)-1, 
    valinit=0, valstep=1, valfmt='%0.0f'
)

ax_button = fig.add_axes([0.80, 0.04, 0.08, 0.05])
btn_play = Button(ax_button, 'Play', color='#1f538d', hovercolor='#14375d')
btn_play.label.set_color('white')

is_updating = False
playing = False

# --- Robust Bounds Helper ---
def get_valid_min_max(arr):
    """Filters out extreme numerical outliers (like 5e30) before returning bounds."""
    arr_clean = arr[np.isfinite(arr)]
    if len(arr_clean) == 0:
        return 0.0, 1.0
    if len(arr_clean) < 5:
        return arr_clean.min(), arr_clean.max()
        
    q25, q75 = np.percentile(arr_clean, [5, 95])
    iqr = q75 - q25
    
    if iqr == 0:
        return arr_clean.min(), arr_clean.max()
        
    # Keep anything within a very generous 5x IQR (keeps extreme flight maneuvers, drops mathematical glitches)
    valid_data = arr_clean[(arr_clean >= q25 - 5 * iqr) & (arr_clean <= q75 + 5 * iqr)]
    
    if len(valid_data) == 0:
        return arr_clean.min(), arr_clean.max()
        
    return valid_data.min(), valid_data.max()

def update_plot(frame_idx):
    global is_updating
    if is_updating: return
    is_updating = True
    
    idx = int(frame_idx)
    
    # 1. Dynamic bounds calculation ignoring extreme outliers
    curr_e = plot_e[:idx+1]
    curr_n = plot_n[:idx+1]
    curr_u = plot_u[:idx+1]
    
    e_min, e_max = get_valid_min_max(curr_e)
    n_min, n_max = get_valid_min_max(curr_n)
    u_min, u_max = get_valid_min_max(curr_u)
    
    max_range = np.max([e_max - e_min, n_max - n_min, u_max - u_min]) / 2.0
    if max_range < 1.0: max_range = 1.0
    
    mid_e = (e_max + e_min) * 0.5
    mid_n = (n_max + n_min) * 0.5
    mid_u = (u_max + u_min) * 0.5
    
    ax1.set_xlim(mid_e - max_range, mid_e + max_range)
    ax1.set_ylim(mid_n - max_range, mid_n + max_range)
    ax1.set_zlim(mid_u - max_range, mid_u + max_range)

    curr_t = t_hist[:idx+1]
    curr_temp = temp_hist[:idx+1]
    curr_press = press_hist[:idx+1]

    # Time bounds (no need for outlier filtering on time)
    c_t_min, c_t_max = curr_t[0], curr_t[-1]
    if c_t_max == c_t_min: c_t_max = c_t_min + 1
    ax_temp.set_xlim(c_t_min, c_t_max)
    ax_press.set_xlim(c_t_min, c_t_max)
    
    # 2D Y-axis bounds ignoring extreme outliers
    temp_min, temp_max = get_valid_min_max(curr_temp)
    if temp_max == temp_min:
        ax_temp.set_ylim(temp_min - 1, temp_max + 1)
    else:
        t_pad = (temp_max - temp_min) * 0.1
        ax_temp.set_ylim(temp_min - t_pad, temp_max + t_pad)
        
    press_min, press_max = get_valid_min_max(curr_press)
    if press_max == press_min:
        ax_press.set_ylim(press_min - 10, press_max + 10)
    else:
        p_pad = (press_max - press_min) * 0.1
        ax_press.set_ylim(press_min - p_pad, press_max + p_pad)
    
    # 2. Update Path (we still map the raw data to the line, just the axes ignore the spike)
    trail_line.set_data_3d(curr_e, curr_n, curr_u)
    
    # 3. Update Orientation
    R = euler_to_matrix(yaw_hist[idx], pitch_hist[idx], roll_hist[idx])
    axis_len = max_range * 0.15 
    verts_scaled = base_verts * (axis_len * 0.4)
    v_plot_1 = body_to_plot_frame(verts_scaled, R) + np.array([plot_e[idx], plot_n[idx], plot_u[idx]])
    plane1.set_verts([[v_plot_1[i] for i in face] for face in faces_idx])
    
    verts_unit = base_verts * 0.48
    v_plot_2 = body_to_plot_frame(verts_unit, R)
    plane2.set_verts([[v_plot_2[i] for i in face] for face in faces_idx])
    
    # 4. Update 2D Lines
    line_temp.set_data(curr_t, curr_temp)
    line_press.set_data(curr_t, curr_press)
    
    # 5. Sync slider position
    if int(time_slider.val) != idx:
        time_slider.set_val(idx)
        
    fig.canvas.draw_idle()
    is_updating = False

time_slider.on_changed(update_plot)

def toggle_play(event):
    global playing
    if playing:
        playing = False
        btn_play.label.set_text('Play')
    else:
        if time_slider.val >= len(t_hist) - 1:
            time_slider.set_val(0)
        playing = True
        btn_play.label.set_text('Pause')
    fig.canvas.draw_idle()

btn_play.on_clicked(toggle_play)

def anim_step(i):
    global playing
    if not playing:
        return
    idx = int(time_slider.val) + 1
    if idx >= len(t_hist):
        playing = False
        btn_play.label.set_text('Play')
        fig.canvas.draw_idle()
        return
    update_plot(idx)

ani = animation.FuncAnimation(fig, anim_step, interval=100, blit=False, save_count=0)

update_plot(0)
plt.show()