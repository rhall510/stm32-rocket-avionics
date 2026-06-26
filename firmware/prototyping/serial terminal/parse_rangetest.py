import re
import matplotlib.pyplot as plt
import matplotlib.dates as mdates
from datetime import datetime
from collections import defaultdict
import statistics

def calc_rolling_loss(is_missed, window_size=10):
    """Calculates a moving average of missed packets (Packet Error Rate)."""
    if not is_missed:
        return []
    
    rolling_loss = []
    for i in range(len(is_missed)):
        start_idx = max(0, i - window_size + 1)
        window = is_missed[start_idx : i + 1]
        loss_pct = (sum(window) / len(window)) * 100.0
        rolling_loss.append(loss_pct)
    return rolling_loss

def parse_rf_logs(file_path):
    log_pattern = re.compile(r'^\[(\d{2}:\d{2}:\d{2}\.\d+)\]\s+(.*)')

    data_868 = {'time': [], 'seq': [], 'rssi': [], 'all_times': [], 'is_missed': []}
    data_24g = {'time': [], 'seq': [], 'rssi': [], 'snr': [], 'all_times': [], 'is_missed': []}
    
    today = datetime.now()

    with open(file_path, 'r') as f:
        for line in f:
            line = line.strip()
            match = log_pattern.match(line)
            
            if not match:
                continue

            time_str, payload_str = match.groups()
            
            if payload_str.startswith('>'):
                continue

            hex_clean = payload_str.replace(' ', '')
            try:
                hex_bytes = bytes.fromhex(hex_clean)
            except ValueError:
                continue 

            if len(hex_bytes) < 4 or hex_bytes[0:3] != b'\xbe\xeb\x03':
                continue

            payload_len = hex_bytes[3]
            if len(hex_bytes) < 4 + payload_len:
                continue

            payload = hex_bytes[4 : 4 + payload_len]
            if len(payload) == 0:
                continue

            source_byte = payload[0]
            is_timeout = (source_byte & 0x80) != 0
            is_24g = (source_byte & 0x01) != 0

            time_obj = datetime.strptime(time_str, '%H:%M:%S.%f')
            time_obj = time_obj.replace(year=today.year, month=today.month, day=today.day)

            if not is_24g:
                data_868['all_times'].append(time_obj)
                data_868['is_missed'].append(1 if is_timeout else 0)
            else:
                data_24g['all_times'].append(time_obj)
                data_24g['is_missed'].append(1 if is_timeout else 0)

            if is_timeout:
                continue

            if len(payload) < 7:
                continue

            seq = int.from_bytes(payload[2:6], byteorder='big')
            
            rssi = payload[6]
            if rssi > 127:
                rssi -= 256

            if not is_24g:
                data_868['time'].append(time_obj)
                data_868['seq'].append(seq)
                data_868['rssi'].append(rssi)
            else:
                if len(payload) < 8:
                    continue
                
                snr = payload[7]
                if snr > 127:
                    snr -= 256

                data_24g['time'].append(time_obj)
                data_24g['seq'].append(seq)
                data_24g['rssi'].append(rssi)
                data_24g['snr'].append(snr)

    data_868['rolling_loss'] = calc_rolling_loss(data_868['is_missed'], window_size=10)
    data_24g['rolling_loss'] = calc_rolling_loss(data_24g['is_missed'], window_size=10)

    return data_868, data_24g

def plot_rf_data(data_868, data_24g):
    fig, axes = plt.subplots(2, 2, figsize=(14, 10))
    fig.suptitle('Hardware Bring-up Range Test Telemetry', fontsize=16, weight='bold')
    time_fmt = mdates.DateFormatter('%H:%M:%S')

    # Top Left: RSSI over Time (Both Channels) - Markers removed
    ax1 = axes[0, 0]
    ax1.plot(data_868['time'], data_868['rssi'], linestyle='-', color='#1f77b4', alpha=0.8, label='868MHz')
    ax1.plot(data_24g['time'], data_24g['rssi'], linestyle='-', color='#2ca02c', alpha=0.8, label='2.4GHz')
    ax1.set_title('RSSI over Time')
    ax1.set_ylabel('RSSI (dBm)')
    ax1.grid(True, linestyle='--', alpha=0.5)
    ax1.legend()
    ax1.xaxis.set_major_formatter(time_fmt)

    # Top Right: 2.4GHz SNR over Time - Markers removed
    ax2 = axes[0, 1]
    ax2.plot(data_24g['time'], data_24g['snr'], linestyle='-', color='#d62728', alpha=0.8)
    ax2.set_title('2.4GHz SNR over Time')
    ax2.set_ylabel('SNR (dB)')
    ax2.grid(True, linestyle='--', alpha=0.5)
    ax2.xaxis.set_major_formatter(time_fmt)

    # Bottom Left: 2.4GHz RSSI vs SNR (Mean with Error Band)
    ax3 = axes[1, 0]
    
    # Group RSSI values by their corresponding SNR
    snr_groups = defaultdict(list)
    for snr_val, rssi_val in zip(data_24g['snr'], data_24g['rssi']):
        snr_groups[snr_val].append(rssi_val)
        
    sorted_snrs = sorted(snr_groups.keys())
    mean_rssi = []
    std_rssi = []
    
    for snr_val in sorted_snrs:
        rssi_list = snr_groups[snr_val]
        mean_rssi.append(statistics.mean(rssi_list))
        # Need at least two data points to calculate standard deviation
        if len(rssi_list) > 1:
            std_rssi.append(statistics.stdev(rssi_list))
        else:
            std_rssi.append(0.0)

    # Calculate the upper and lower bounds for the error band
    upper_bound = [m + s for m, s in zip(mean_rssi, std_rssi)]
    lower_bound = [m - s for m, s in zip(mean_rssi, std_rssi)]

    ax3.plot(sorted_snrs, mean_rssi, color='#9467bd', linewidth=2, label='Mean RSSI')
    ax3.fill_between(sorted_snrs, lower_bound, upper_bound, color='#9467bd', alpha=0.3, label='±1 Std Dev')
    
    ax3.set_title('2.4GHz RSSI vs SNR')
    ax3.set_xlabel('SNR (dB)')
    ax3.set_ylabel('RSSI (dBm)')
    ax3.grid(True, linestyle='--', alpha=0.5)
    ax3.legend()

    # Bottom Right: Proportion of Missed Packets (Rolling Window)
    ax4 = axes[1, 1]
    ax4.plot(data_868['all_times'], data_868['rolling_loss'], linestyle='-', color='#1f77b4', label='868MHz (10-pkt avg)')
    ax4.plot(data_24g['all_times'], data_24g['rolling_loss'], linestyle='-', color='#2ca02c', label='2.4GHz (10-pkt avg)')
    ax4.set_title('Packet Error Rate (% Missed)')
    ax4.set_ylabel('Loss (%)')
    ax4.set_ylim(-5, 105)
    ax4.grid(True, linestyle='--', alpha=0.5)
    ax4.legend()
    ax4.xaxis.set_major_formatter(time_fmt)

    # Angle the x-axis timestamps for readability
    for ax in [ax1, ax2, ax4]:
        plt.setp(ax.xaxis.get_majorticklabels(), rotation=45, ha='right')

    plt.tight_layout()
    plt.show()

if __name__ == "__main__":
    log_file = 'test.txt' 
    d_868, d_24g = parse_rf_logs(log_file)
    plot_rf_data(d_868, d_24g)