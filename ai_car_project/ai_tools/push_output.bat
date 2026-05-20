@echo off

echo "..................."
echo "这是推送AI小车项目输出文件到设备的脚本"
echo "..................."

adb push ../ai_car_output /mnt/car_test
adb push quecpi_smartcar/service_config/run/wifi_conf.py /etc/wifi/wifi_conf.py
adb push quecpi_smartcar/service_config/run/main.conf /etc/bluetooth/main.conf
adb shell "cd /mnt/car_test/ros2_ws && source /usr/ros/humble/setup.bash && colcon build"
adb shell "cd /mnt/car_test/ros2_ws && source install/setup.bash"
adb shell "chmod +x /mnt/car_test/quecpi_smartcar/service_config/run/deploy_service_config.sh"
adb shell "chmod +x /mnt/car_test/AI_Talker/BIN/ai_car_demo"
adb shell "chmod +x /etc/wifi/wifi_conf.py"
adb shell "source /mnt/car_test/quecpi_smartcar/service_config/run/deploy_service_config.sh" > temp_log.txt 2>&1

findstr /c:"pip3 install gpiod netifaces pyserial" temp_log.txt >nul && (
    adb shell "nmcli dev wifi connect ""Quectel-Customer-2.4G"" password ""Customer-Quectel"""
	for /f "delims=" %%a in ('powershell -Command "(Invoke-WebRequest -Uri \"http://www.baidu.com\" -UseBasicParsing).Headers['Date']"') do (
		adb shell "date -s \"%%a\""
	)
	adb shell date
	echo "deal fix_python_dependencies.sh"
	adb shell "chmod +x /mnt/car_test/quecpi_smartcar/service_config/run/fix_python_dependencies.sh"
	adb shell "source /mnt/car_test/quecpi_smartcar/service_config/run/fix_python_dependencies.sh"
	adb shell "source /mnt/car_test/quecpi_smartcar/service_config/run/deploy_service_config.sh"
) || (
    echo success
)
adb shell "systemctl enable /mnt/car_test/quecpi_smartcar/service_config/service/start_car.service"
adb shell sync

echo Operation completed, device is restarting ...
pause
