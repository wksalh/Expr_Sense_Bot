@echo off
setlocal enabledelayedexpansion
echo "..................."
echo "这是推送AI小车项目提取log的脚本"
echo "..................."
adb pull /mnt/car_test/quecpi_smartcar/service_config/run/log log
adb pull /mnt/car_test/AI_Talker/BIN/ai_audio ai_audio

adb shell rm /mnt/car_test/quecpi_smartcar/service_config/run/log/* -rf
adb shell rm /mnt/car_test/AI_Talker/BIN/ai_audio/* -rf


adb shell reboot

echo Operation completed, device is restarting ..
pause
