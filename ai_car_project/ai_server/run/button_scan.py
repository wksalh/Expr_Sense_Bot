#!/usr/bin/env python3
import os
import time
import subprocess

# --- 硬件与路径配置 ---
KEY1_PIN = 78
KEY2_PIN = 32
CONTROL_FILE = "/tmp/ble_netcfg_control"

# 配网配置文件所在目录 (AI_Talker 相关)
def get_ai_talker_bin_path():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    # 按照你原始脚本的逻辑：run -> service_config -> quecpi_smartcar -> 父目录 -> AI_Talker/BIN
    path = os.path.abspath(os.path.join(script_dir, '..', '..', '..', 'AI_Talker', 'BIN'))
    return path if os.path.isdir(path) else None

def get_command_output(cmd):
    """执行系统命令并获取输出结果"""
    try:
        result = subprocess.run(cmd, shell=True, capture_output=True, text=True, timeout=3)
        return result.stdout.strip()
    except Exception:
        return None

def init_bluetooth_simple():
    """
    蓝牙初始化：先禁用 ecred，重启蓝牙服务，再执行 hciconfig hci0 up
    """
    print("\n[蓝牙初始化] 开始初始化流程")
    
    # 1. 禁用 ecred
    try:
        result = subprocess.run(
            "echo N > /sys/module/bluetooth/parameters/enable_ecred",
            shell=True,
            capture_output=True,
            text=True,
            timeout=3,
        )
        if result.returncode == 0:
            print("[蓝牙初始化] 已禁用 ecred")
        else:
            print(f"[蓝牙初始化] 禁用 ecred 失败: {result.stderr}")
    except Exception as e:
        print(f"[蓝牙初始化] 禁用 ecred 异常: {e}")
    
    # 2. 重启蓝牙服务
    try:
        result = subprocess.run(
            ["systemctl", "restart", "bluetooth"],
            capture_output=True,
            text=True,
            timeout=10,
        )
        if result.returncode == 0:
            print("[蓝牙初始化] bluetooth 服务重启成功")
        else:
            print(f"[蓝牙初始化] bluetooth 服务重启失败: {result.stderr}")
        time.sleep(1)  # 等待服务完全启动
    except Exception as e:
        print(f"[蓝牙初始化] 重启 bluetooth 服务异常: {e}")
    
    # 3. 执行 hciconfig hci0 up
    print("[蓝牙初始化] 执行: hciconfig hci0 up")
    try:
        result = subprocess.run(
            ["hciconfig", "hci0", "up"],
            capture_output=True,
            text=True,
            timeout=5,
        )
        if result.returncode == 0:
            print("[蓝牙初始化] hci0 up 成功")
            return True

        message = (result.stderr or result.stdout or "").strip()
        print(f"[蓝牙初始化] hci0 up 失败: {message}")
        return False
    except Exception as e:
        print(f"[蓝牙初始化] hci0 up 异常: {e}")
        return False

def trigger_ble_config():
    """Key1 触发：初始化蓝牙并开启配网"""
    # 无论当前状态如何，都执行完整的蓝牙初始化流程
    if not init_bluetooth_simple():
        print("[配网业务] 蓝牙初始化失败，终止配网流程")
        return

    try:
        with open(CONTROL_FILE, 'w') as f:
            f.write('start\n')
        print(f"[配网业务] 已发送启动信号到 {CONTROL_FILE}")
    except Exception as e:
        print(f"[配网业务] 写入失败: {e}")

def clear_netcfg_files():
    """Key2 触发：清除配网配置文件"""
    print('\n[清除配置] 正在清理配网信息...')
    
    # 1. 清理 AI_Talker/BIN 下的配置
    ai_talker_bin = get_ai_talker_bin_path()
    config_files = ['qth_config.bin', 'qth_config_BAK.bin']
    
    if ai_talker_bin:
        for f_name in config_files:
            file_path = os.path.join(ai_talker_bin, f_name)
            if os.path.exists(file_path):
                try:
                    os.remove(file_path)
                    print(f"  已删除: {file_path}")
                except Exception as e:
                    print(f"  删除失败 {f_name}: {e}")
    
    # 2. 清理系统 WiFi 配置
    os.system("rm /etc/wifi/* -rf > /dev/null 2>&1")
    # 重启 WiFi 服务以使清理生效
    os.system("systemctl restart wifi.service > /dev/null 2>&1")
    print("[清除配置] 清理完成！")

if __name__ == "__main__":
    # GPIO 初始化
    os.system("rgs c 999 go 4")
    os.system(f"rgs c 999 gsi 0 {KEY1_PIN}")
    os.system(f"rgs c 999 gsi 0 {KEY2_PIN}")

    k1_cnt, k2_cnt = 0, 0
    k1_lock, k2_lock = False, False

    print(">>> 扫描脚本已启动 <<<")
    print("Key1 (GPIO 78) 长按 3s: 启动蓝牙配网")
    print("Key2 (GPIO 32) 长按 3s: 清除配网配置")

    while True:
        # 轮询 Key1
        out1 = get_command_output(f"rgs c 999 gr 0 {KEY1_PIN}")
        if out1 == "0":
            k1_cnt += 1
            if k1_cnt >= 60 and not k1_lock: # 60 * 0.05s = 3s
                k1_lock = True
                print("\n[事件] Key1 长按达标")
                trigger_ble_config()
        else:
            k1_cnt, k1_lock = 0, False

        # 轮询 Key2
        out2 = get_command_output(f"rgs c 999 gr 0 {KEY2_PIN}")
        if out2 == "0":
            k2_cnt += 1
            if k2_cnt >= 60 and not k2_lock: # 60 * 0.05s = 3s
                k2_lock = True
                print("\n[事件] Key2 长按达标")
                clear_netcfg_files()
        else:
            k2_cnt, k2_lock = 0, False

        time.sleep(0.05)
