import serial
import struct
import time
import math

# --- Protocol Constants ---
SYNC_WORD = b'\xBE\xEB'
MSG_TYPE_INFO = 0x01
MSG_TYPE_DOWNLOAD = 0x06

class AvionicsDownloader:
    def __init__(self, port, baudrate=115200):
        self.port = port
        self.baudrate = baudrate
        self.ser = None
        
        self.received_packets = {}  
        self.expected_bytes = 0
        self.total_storage_packets = 0
        self.bytes_per_packet = None
        
        self.raw_log_file = None

    def connect(self):
        self.ser = serial.Serial(self.port, self.baudrate)
        print(f"Connected to {self.port} at {self.baudrate} baud.")

    def disconnect(self):
        if self.ser and self.ser.is_open:
            self.ser.close()

    def _serial_read(self, size):
        data = self.ser.read(size)
        if data and self.raw_log_file:
            self.raw_log_file.write(data)
            self.raw_log_file.flush() 
        return data

    def send_download_cmd(self, amount, offset):
        payload = struct.pack('>II', amount, offset)
        header = SYNC_WORD + bytes([MSG_TYPE_DOWNLOAD, len(payload)])
        cmd_bytes = header + payload
        
        hex_str = ' '.join([f"{b:02X}" for b in cmd_bytes])
        print(f"[TX] {hex_str} (Amount: {amount}, Offset: {offset})")
        
        self.ser.write(cmd_bytes)
        self.ser.flush()

    def read_packet(self, timeout):
        self.ser.timeout = timeout
        sync_buffer = bytearray()
        
        while True:
            b = self._serial_read(1)
            if not b:
                return None  
            sync_buffer.extend(b)
            if len(sync_buffer) >= 2 and sync_buffer[-2:] == SYNC_WORD:
                break
                
        header = self._serial_read(2)
        if len(header) < 2:
            return None
        msg_type, payload_len = header[0], header[1]
        
        payload = self._serial_read(payload_len)
        if len(payload) < payload_len:
            return None
            
        return msg_type, payload

    def _get_contiguous_missing_blocks(self, start_seq, end_seq):
        """Finds missing sequence numbers within a specific range and groups them."""
        missing_seqs = [i for i in range(start_seq, end_seq) if i not in self.received_packets]
        if not missing_seqs:
            return []

        blocks = []
        current_start = missing_seqs[0]
        count = 1

        for i in range(1, len(missing_seqs)):
            if missing_seqs[i] == missing_seqs[i-1] + 1:
                count += 1
            else:
                blocks.append((current_start, count))
                current_start = missing_seqs[i]
                count = 1
        blocks.append((current_start, count))
        
        return blocks

    def _receive_data_loop(self, current_offset=0, initial_timeout=3.0, inter_packet_timeout=0.5):
        """
        Listens for data packets. Uses a longer timeout for the VERY FIRST data packet 
        to account for LoRa setup latency, then tightens the timeout.
        """
        is_first_packet = True
        
        while True:
            current_timeout = initial_timeout if is_first_packet else inter_packet_timeout
            pkt = self.read_packet(timeout=current_timeout)
            
            if not pkt:
                break 
                
            msg_type, payload = pkt
            
            # Only process MSG_TYPE_DOWNLOAD (Ignore the INFO packets sent at the start of blocks)
            if msg_type == MSG_TYPE_DOWNLOAD and len(payload) >= 4:
                
                # We have received actual data! Safe to drop the timeout for subsequent packets.
                is_first_packet = False
                
                relative_seq_num, = struct.unpack('>I', payload[:4])
                data = payload[4:]
                
                if self.bytes_per_packet is None and len(data) > 0:
                    self.bytes_per_packet = len(data)
                
                absolute_seq_num = int(current_offset // self.bytes_per_packet) + relative_seq_num
                
                self.received_packets[absolute_seq_num] = data
                
                current_bytes = sum(len(v) for v in self.received_packets.values())
                if current_bytes >= self.expected_bytes:
                    break

    def download(self, output_filename="avionics_data.bin", raw_log_filename="raw_serial_log.bin", request_amount=0, request_offset=0):
        if not self.ser or not self.ser.is_open:
            self.connect()

        self.received_packets.clear()
        self.bytes_per_packet = None

        print(f"\n--- Starting Avionics Download (Amount: {request_amount}, Offset: {request_offset}) ---")
        
        with open(raw_log_filename, 'wb') as self.raw_log_file:
            print(f"Logging raw serial bytes to {raw_log_filename}...")
            
            self.send_download_cmd(amount=request_amount, offset=request_offset)

            print("Waiting for controller info...")
            while True:
                pkt = self.read_packet(timeout=3.0)
                if not pkt:
                    print("Error: Timeout waiting for info packet. Aborting.")
                    return
                    
                msg_type, payload = pkt
                if msg_type == MSG_TYPE_INFO and len(payload) == 8:
                    self.expected_bytes, self.total_storage_packets = struct.unpack('>II', payload)
                    print(f"Info: {self.expected_bytes} bytes to download.")
                    print(f"Info: {self.total_storage_packets} total packets in avionics storage.")
                    if self.expected_bytes == 0:
                        print("No data to download.")
                        return
                    break

            print("Receiving initial data burst...")
            self._receive_data_loop(current_offset=request_offset, initial_timeout=3.0, inter_packet_timeout=0.5)

            if self.bytes_per_packet is None:
                print("Error: No data packets received in the initial burst.")
                return

            # Calculate the absolute sequence boundaries for this specific chunk
            start_seq = request_offset // self.bytes_per_packet
            end_seq = math.ceil((request_offset + self.expected_bytes) / self.bytes_per_packet)
            
            while True:
                blocks = self._get_contiguous_missing_blocks(start_seq, end_seq)
                
                if not blocks:
                    print("All data successfully received!")
                    break
                    
                missing_packet_count = sum(count for _, count in blocks)
                print(f"Missing {missing_packet_count} packets across {len(blocks)} blocks. Requesting...")

                for b_start_seq, count in blocks:
                    offset = b_start_seq * self.bytes_per_packet
                    amount = count * self.bytes_per_packet
                    
                    # Cap the amount if it exceeds our requested chunk window
                    if offset + amount > (request_offset + self.expected_bytes):
                        amount = (request_offset + self.expected_bytes) - offset
                        
                    self.send_download_cmd(amount, offset)
                    self._receive_data_loop(current_offset=offset, initial_timeout=3.0, inter_packet_timeout=0.5)

        self.raw_log_file = None 

        print(f"\nWriting final parsed data to {output_filename}...")
        with open(output_filename, 'wb') as f:
            # We only write the sequences we actually requested to the file
            for i in range(start_seq, end_seq):
                if i in self.received_packets:
                    f.write(self.received_packets[i])
                else:
                    print(f"Warning: Sequence {i} is still missing at write time.")
                    
        print("Done.")

if __name__ == "__main__":
    PORT = 'COM6'

    downloader = AvionicsDownloader(port=PORT, baudrate=115200)
    try:
        downloader.download(
            output_filename="flight_data.bin",
            raw_log_filename="raw_serial_log.bin",
            request_amount=0,
            request_offset=0
        )
    except Exception as e:
        print(f"An error occurred: {e}")
    finally:
        downloader.disconnect()