import serial, time, threading

def monitor(port):
    try:
        # Open port
        ser = serial.Serial(port, 115200, timeout=1)
        # Reset the ESP32 (DTR/RTS sequence)
        ser.setDTR(False)
        ser.setRTS(False)
        time.sleep(0.1)
        ser.setDTR(True)
        ser.setRTS(True)
        time.sleep(0.1)
        ser.setDTR(False)
        ser.setRTS(False)
        
        print(f"Listening on {port}...")
        start = time.time()
        while time.time() - start < 45:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if line:
                print(f"[{port}] {line}")
                if "Avg RTT:" in line:
                    # Read 5 more lines to get the end of the box
                    for _ in range(5):
                        extra = ser.readline().decode('utf-8', errors='ignore').strip()
                        if extra:
                            print(f"[{port}] {extra}")
                    break
        ser.close()
    except Exception as e:
        print(f"[{port}] Error: {e}")

t1 = threading.Thread(target=monitor, args=('COM4',))
t2 = threading.Thread(target=monitor, args=('COM6',))
t1.start()
t2.start()
t1.join()
t2.join()
