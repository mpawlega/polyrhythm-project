#!/usr/bin/env python3
import subprocess
import concurrent.futures

FIRMWARE = "build/esp8266.esp8266.d1_mini_clone/strip_test.ino.bin"
ESPOTA = "/Users/rbgorbet/Library/Arduino15/packages/esp8266/hardware/esp8266/3.0.2/tools/espota.py"   # path to your espota.py if needed
PORT = "8266"

def flash_one(host):
    cmd = [
        "python3", ESPOTA,
        "--ip", host,
        "--port", PORT,
        "--auth=",            # empty auth
        "--file", FIRMWARE
    ]
    print(f"\n=== Flashing {host} ===")
    try:
        subprocess.check_call(cmd)
        print(f"OK: {host}")
        return (host, True)
    except subprocess.CalledProcessError:
        print(f"FAIL: {host}")
        return (host, False)

def main():
    with open("nodes.txt") as f:
        hosts = [line.strip() for line in f if line.strip()]

    # Choose mode
    SEQUENTIAL = True  # set to False for parallel

    if SEQUENTIAL:
        for h in hosts:
            flash_one(h)
    else:
        with concurrent.futures.ThreadPoolExecutor(max_workers=16) as ex:
            list(ex.map(flash_one, hosts))

if __name__ == "__main__":
    main()

