@echo off
echo ========================================================
echo Monitoring ESP32 on COM4
echo Press Ctrl+] to exit
echo ========================================================
python -m serial.tools.miniterm COM4 115200
