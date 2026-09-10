@echo off
echo ========================================================
echo Flashing ESP32 on COM4
echo ========================================================
echo IMPORTANT: When you see "Connecting...", press and HOLD
echo the "BOOT" button on the ESP32 until the upload starts!
echo ========================================================
pause

python -m esptool --chip esp32 --port COM4 --baud 460800 write_flash -z --flash_mode dio --flash_freq 40m --flash_size detect 0x1000 build/bootloader/bootloader.bin 0x8000 build/partition_table/partition-table.bin 0x10000 build/benchmark_c.bin

echo.
echo Done!
pause
