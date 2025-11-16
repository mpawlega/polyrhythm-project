import socket

DEBUG_PORT = 8890  # pick an unused port different from your download port

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(('', DEBUG_PORT))
print(f"Listening for /download UDP debug packets on port {DEBUG_PORT}...")

try:
    while True:
        data, addr = sock.recvfrom(2048)
        text = data.decode(errors='ignore')
        if text.startswith("/download"):
            print(f"From {addr}: {text.strip()}")
except KeyboardInterrupt:
    print("Exiting listener")

