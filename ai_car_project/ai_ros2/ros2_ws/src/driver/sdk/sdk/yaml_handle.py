import yaml
import os

# 获取包路径
package_path = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
# 配置文件路径 - 使用正确的相对路径计算
# 从 /mnt/ros2_ws/install/sdk/lib/python3.10 到 /mnt/software/lab_tool/ 需要 ../../../../../software/lab_tool/
lab_file_path = os.path.abspath(os.path.join(package_path, '..', '..', '..', '..', '..', 'software', 'lab_tool', 'lab_config.yaml'))
servo_file_path = os.path.abspath(os.path.join(package_path, '..', '..', '..', '..', '..', 'software', 'lab_tool', 'servo_config.yaml'))

def get_yaml_data(yaml_file):
    file = open(yaml_file, 'r', encoding='utf-8')
    file_data = file.read()
    file.close()
    
    data = yaml.load(file_data, Loader=yaml.FullLoader)
    
    return data

def save_yaml_data(data, yaml_file):
    file = open(yaml_file, 'w', encoding='utf-8')
    yaml.dump(data, file)
    file.close()
