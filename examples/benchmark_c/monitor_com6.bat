@echo off
echo ========================================================
echo Monitoring ESP32 on COM6
echo Press Ctrl+] to exit
echo ========================================================
python -m serial.tools.miniterm COM6 115200
