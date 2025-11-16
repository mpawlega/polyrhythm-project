import socket
import struct
import time
import numpy as np
import csv

SEND_PORT = 8888      # port to send commands to nodes
RECV_PORT = 8887      # local port to listen for responses
CHUNK_SIZE = 64       # matches Arduino chunk size
REQUEST_DELAY = 0.10  # seconds to wait between chunk requests (throttling)

def load_nodes(filename="nodes.txt"):
    nodes = []
    with open(filename, "r") as f:
        for line in f:
            line = line.strip()
            if line:
                nodes.append((line, SEND_PORT))
    return nodes

def send_command(sock, ip, command):
    sock.sendto(command.encode('ascii'), (ip, SEND_PORT))

def receive_samples(sock, expected_chunks, timeout=2):
    sock.settimeout(timeout)
    chunks = []
    try:
        for _ in range(expected_chunks):
            data, addr = sock.recvfrom(1500)

            if is_non_sample_packet(data):
                 continue  # skip this packet

            chunks.append(data)
    except socket.timeout:
        pass
    return chunks

def is_non_sample_packet(packet: bytes) -> bool:
    """Return True if this packet is a debug/status/data message, not sample data."""
    if not packet:
        return True

    first = packet[:1]
    # '/' or '\' (debug/status messages)
    if first in (b"/", b"\\"):
        try:
            # decode only first ~20 bytes to avoid heavy work
            head = packet[:20].decode("utf-8", errors="ignore")
            if head.startswith(("/debug", "/status", "/data")):
                return True
        except:
            pass

    return False

def parse_samples(data):
    # data format: [4 bytes start_index BE][2 bytes count BE][count * 8 bytes timestamps BE]
    if len(data) < 6:
        return None, []
    start_idx = struct.unpack(">I", data[0:4])[0]
    count = struct.unpack(">H", data[4:6])[0]
    timestamps = []
    for i in range(count):
        offset = 6 + i * 8
        if offset + 8 <= len(data):
            ts = struct.unpack(">Q", data[offset:offset+8])[0]
            timestamps.append(ts)
        else:
            break  # incomplete timestamp at end of packet
    return start_idx, timestamps

def fetch_all_samples(sock, node_ip, chunk_size=64, timeout=2.0):
    samples = []
    start_idx = 0
    sock.settimeout(timeout)

    while True:
        # Send 'R' command + start index to request a chunk of samples
        cmd = f"R{start_idx}"
        sock.sendto(cmd.encode(), (node_ip, SEND_PORT))

        try:
            data, _ = sock.recvfrom(1500)
        except socket.timeout:
            print(f"Timeout waiting for data from {node_ip} at index {start_idx}")
            break

        if is_non_sample_packet(data):
            continue

        pkt_start_idx, timestamps = parse_samples(data)

        if pkt_start_idx != start_idx:
            print(f"Warning: packet start index {pkt_start_idx} does not match requested {start_idx}")

        if not timestamps:
            print(f"No samples returned at index {start_idx}, stopping.")
            break

        samples.extend(timestamps)

        # If fewer samples received than chunk_size, must be last packet
        if len(timestamps) < chunk_size:
            break

        start_idx += chunk_size

        # Add throttling to avoid UDP congestion
        time.sleep(REQUEST_DELAY)

    return samples

def analyze_samples(samples):
    arr = np.array(samples, dtype=np.float64) * 1e-6
    if len(arr) < 2:
        return {'samples': len(arr), 'drift_s': 0, 'jitter_s': 0}

    x = np.arange(len(arr))
    coeffs = np.polyfit(x, arr, 1)
    drift_s = coeffs[0]
    residuals = arr - np.polyval(coeffs, x)
    jitter_s = np.std(residuals)
    return {'samples': len(arr), 'drift_s': drift_s, 'jitter_s': jitter_s}

def write_csv(all_nodes_samples, filename='samples.csv'):
    max_len = max(len(s) for s in all_nodes_samples.values())
    ips = list(all_nodes_samples.keys())
    with open(filename, 'w', newline='') as csvfile:
        writer = csv.writer(csvfile)
        writer.writerow(['Sample Index'] + ips)
        for i in range(max_len):
            row = [i]
            for ip in ips:
                samples = all_nodes_samples[ip]
                row.append(samples[i] if i < len(samples) else '')
            writer.writerow(row)
    print(f"Wrote samples CSV to {filename}")

def main():
    print("Starting UDP sample download and analysis")
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(('', RECV_PORT))

    # Load nodes from nodes.txt
    nodes = load_nodes()
    print(f"Loaded {len(nodes)} nodes from nodes.txt")

    all_nodes_samples = {}
    stats = {}

    for ip, _ in nodes:
        print(f"Requesting samples from node {ip}...")
        samples = fetch_all_samples(sock, ip)
        print(f"Received {len(samples)} samples from {ip}")
        all_nodes_samples[ip] = samples
        stats[ip] = analyze_samples(samples)

        # Throttle between nodes too
        time.sleep(0.2)

    print("\nNode stats:")
    for ip, stat in stats.items():
        print(f"{ip}: Samples={stat['samples']}, Drift (s/sample)={stat['drift_s']:.6e}, Jitter (s)={stat['jitter_s']:.6e}")

    write_csv(all_nodes_samples)

if __name__ == "__main__":
    main()

