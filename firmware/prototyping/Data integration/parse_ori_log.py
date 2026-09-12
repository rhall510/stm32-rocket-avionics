import numpy as np
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from mpl_toolkits.mplot3d.art3d import Poly3DCollection
import time
import os

# Set this to the name of your saved log file
LOG_FILE = 'avlog.txt'

def parse_log(filepath):
    quaternions = []
    if not os.path.exists(filepath):
        print(f"Error: Could not find {filepath}")
        return np.array([])
        
    with open(filepath, 'r') as f:
        for line in f:
            if line.startswith("ORI:"):
                # Split at 'ORI:' and parse the comma-separated floats
                parts = line.strip().split("ORI:")[1].split(",")
                q = [float(p.strip()) for p in parts]
                quaternions.append(q)
                
    return np.array(quaternions)

def quat_to_matrix(q):
    qw, qx, qy, qz = q
    qx2, qy2, qz2 = qx*qx, qy*qy, qz*qz

    R = np.array([
        [1 - 2*qy2 - 2*qz2,     2*qx*qy - 2*qz*qw,     2*qx*qz + 2*qy*qw],
        [    2*qx*qy + 2*qz*qw, 1 - 2*qx2 - 2*qz2,     2*qy*qz - 2*qx*qw],
        [    2*qx*qz - 2*qy*qw,     2*qy*qz + 2*qx*qw, 1 - 2*qx2 - 2*qy2]
    ])
    return R

def body_to_plot_frame(verts, R):
    v_nav = (R @ verts.T).T
    # Map Math/Nav frame (North, East, Down) to Plot frame (East, North, Up)
    v_plot = np.column_stack((v_nav[:, 1], v_nav[:, 0], -v_nav[:, 2]))
    return v_plot

# 1. Parse quaternions
q_raw_est = parse_log(LOG_FILE)

if len(q_raw_est) == 0:
    print("No ORI: data found to animate.")
    exit()

# 2. Setup synthetic timestamps (assuming ~30Hz logging based on interval=33)
t_q_est = np.arange(len(q_raw_est)) * 0.033

# 3. Setup Plot
fig = plt.figure(figsize=(10, 8))
ax = fig.add_subplot(111, projection='3d')
fig.suptitle("Rocket Avionics Orientation Playback", fontsize=16, fontweight='bold')

ax.set_xlabel('East (X)')
ax.set_ylabel('North (Y)')
ax.set_zlabel('Up (Z)')

ax.set_xlim(-1, 1)
ax.set_ylim(-1, 1)
ax.set_zlim(-1, 1)

# Body Frame Vertices (NED: X=Forward, Y=Right, Z=Down)
base_verts = np.array([
    [ 2.0,  0.0,  0.0],  # 0: Nose
    [-1.0, -1.5,  0.0],  # 1: Left wing tip
    [-1.0,  1.5,  0.0],  # 2: Right wing tip
    [-1.0,  0.0, -0.5],  # 3: Top tail fin (-Z is up in NED)
    [-1.0,  0.0,  0.2]   # 4: Bottom fuselage
])

faces_idx = [
    [0, 1, 3], # 0: Top-left wing-to-fin (TOP)
    [0, 2, 3], # 1: Top-right wing-to-fin (TOP)
    [0, 1, 4], # 2: Bottom-left (BOTTOM)
    [0, 2, 4], # 3: Bottom-right (BOTTOM)
    [1, 2, 3], # 4: Back plate top (TOP)
    [1, 2, 4]  # 5: Back plate bottom (BOTTOM)
]

face_colors = ['saddlebrown', 'saddlebrown', 'orange', 'orange', 'saddlebrown', 'orange']
plane = Poly3DCollection([], facecolors=face_colors, edgecolors='maroon', alpha=0.9)
ax.add_collection3d(plane)

# 4. Animation loop functions
def update(num):
    R = quat_to_matrix(q_raw_est[num])
    
    verts_unit = base_verts * 0.4
    v_plot = body_to_plot_frame(verts_unit, R)
    
    faces = [[v_plot[idx] for idx in face] for face in faces_idx]
    plane.set_verts(faces)
    
    return plane,

def realtime_frames():
    start_real = time.time()
    start_sim = t_q_est[0]
    end_sim = t_q_est[-1]
    
    while True:
        elapsed_real = time.time() - start_real
        target_sim = start_sim + elapsed_real
        
        if target_sim >= end_sim:
            yield len(t_q_est) - 1
            break
            
        idx = np.searchsorted(t_q_est, target_sim)
        yield idx

ani = animation.FuncAnimation(fig, update, frames=realtime_frames,
                              interval=33, blit=False, repeat=False, cache_frame_data=False)

plt.tight_layout()
plt.show()