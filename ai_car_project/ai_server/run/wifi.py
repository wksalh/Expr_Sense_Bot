#!/usr/bin/env python3
import os
import sys
import time
import logging
import threading
import subprocess

led1_pin = 34
led2_pin = 35 
#chip = gpiod.Chip('gpiochip0')
#led1 = chip.get_line(led1_pin)
#led1.request(consumer="led1", type=gpiod.LINE_REQ_DIR_OUT)
#led2 = chip.get_line(led2_pin)
#led2.request(consumer="led2", type=gpiod.LINE_REQ_DIR_OUT)
#led1.set_value(0)
#led2.set_value(0)

os.system("rgs c 999 go 4")
os.system("rgs c 999 gso 0 34")
os.system("rgs c 999 gso 0 35")
os.system("rgs c 999 gw 0 34 0")
os.system("rgs c 999 gw 0 35 0")

path = os.path.split(os.path.realpath(__file__))[0]
log_file_path = "%s/wifi.log"%path
if not os.path.exists(log_file_path):
    os.system('touch %s'%log_file_path)
# WiFi 指示灯：常亮=未连接局域网，闪烁=已连接局域网
wifi_connected = False
BLINK_ON_MS = 100   # 闪烁时亮 100*10ms=1s
BLINK_OFF_MS = 100  # 闪烁时灭 100*10ms=1s

def led_thread():
    """LED2(pin35)：未连接时常亮，已连接时闪烁"""
    global wifi_connected
    count = 0
    while True:
        if not wifi_connected:
            os.system("rgs c 999 gw 0 35 0")  # 未连接：常亮
        else:
            cycle = BLINK_ON_MS + BLINK_OFF_MS
            if count < BLINK_ON_MS:
                os.system("rgs c 999 gw 0 35 0")  # 亮
            else:
                os.system("rgs c 999 gw 0 35 1")  # 灭
            count = (count + 1) % cycle
        time.sleep(0.01)

if __name__ == "__main__":
    time.sleep(10)

    WIFI_LED = True  # 是否启用 WiFi 指示灯

    logger = logging.getLogger("WiFi tool")
    logger.setLevel(logging.DEBUG)
    log_handler = logging.FileHandler(log_file_path)
    log_handler.setLevel(logging.INFO)
    log_formatter = logging.Formatter('%(name)s - %(asctime)s - %(levelname)s - %(message)s')
    log_handler.setFormatter(log_formatter)
    logger.addHandler(log_handler)

    def check_wifi_connection_status():
        """使用 nmcli dev status 检查WiFi连接状态"""
        try:
            # 获取设备状态
            result = subprocess.run(['nmcli', 'dev', 'status'], 
                                  stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            if result.returncode != 0:
                logger.error("Failed to get device status: %s", result.stderr.decode().strip())
                return False, None
            
            dev_status = result.stdout.decode()
            logger.debug("Device status: %s", dev_status)

            lines = dev_status.strip().split('\n')
            wlan_status = None
            for line in lines:
                if 'wlan0' in line:
                    wlan_status = line
                    break

            if not wlan_status:
                logger.error("wlan0 device not found")
                return False, None

            logger.debug("wlan0 status: %s", wlan_status)

            # 解析 STATE 列（第3列），避免 "disconnected" 被误判为已连接
            parts = wlan_status.split()
            state = parts[2] if len(parts) >= 3 else ""
            if state != 'connected':
                logger.debug("wlan0 state is '%s', not connected", state)
                return False, None

            # 已连接，检查是否为本机热点（AP 模式不算局域网）
            connection_name = parts[3] if len(parts) >= 4 else ""
            if connection_name.startswith("HW-Quec-"):
                logger.debug("wlan0 in AP mode (connection=%s), not considered as LAN connected", connection_name)
                return False, None

            # 获取连接的 SSID
            conn_result = subprocess.run(['nmcli', '-t', '-f', 'ACTIVE,SSID', 'dev', 'wifi'],
                                         stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            if conn_result.returncode == 0:
                conn_info = conn_result.stdout.decode().strip()
                logger.debug("WiFi connection info: %s", conn_info)
                if 'yes:' in conn_info:
                    ssid = conn_info.split('yes:')[1].strip()
                    return True, ssid
                wifi_list_result = subprocess.run(['nmcli', '-t', '-f', 'IN-USE,SSID', 'dev', 'wifi'],
                                                  stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                if wifi_list_result.returncode == 0:
                    wifi_list = wifi_list_result.stdout.decode().strip()
                    logger.debug("WiFi list: %s", wifi_list)
                    for line in wifi_list.split('\n'):
                        if line.strip() and line.startswith('*:'):
                            ssid = line.split('*:')[1].strip()
                            if ssid:
                                return True, ssid
                return True, None
            logger.error("Failed to get WiFi connection info")
            return True, None
                
        except Exception as e:
            logger.error("Error in check_wifi_connection_status(): %s", str(e))
            return False, None

    if WIFI_LED:
        t = threading.Thread(target=led_thread, daemon=True)
        t.start()

    logger.info("WiFi LED: solid on = not connected, blinking = connected to LAN (detection only, no connect/AP)")
    while True:
        try:
            connected, ssid = check_wifi_connection_status()
            wifi_connected = connected
            if connected and ssid:
                logger.debug("WiFi connected to: %s", ssid)
        except BaseException as e:
            logger.error("Check WiFi error: %s", e)
            wifi_connected = False
        time.sleep(2)
