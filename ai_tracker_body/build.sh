#!/bin/bash

# 设置环境变量
export LD_LIBRARY_PATH=./lib:$LD_LIBRARY_PATH
export ADSP_LIBRARY_PATH="./lib/;/system/lib/rfsa/adsp;/system/vendor/lib/rfsa/adsp;/dsp"

echo "Building ai_tracker_body Python module..."

# 检查必要文件
if [ ! -f "tracker_final.cpp" ]; then
    echo "Error: tracker_final.cpp not found!"
    exit 1
fi

if [ ! -d "include" ]; then
    echo "Error: include directory not found!"
    exit 1
fi

if [ ! -d "lib" ]; then
    echo "Error: lib directory not found!"
    exit 1
fi

# 修复换行符（如果是从Windows传输的文件）
sed -i 's/\r$//' build.sh 2>/dev/null

# 使用setup.py构建
python3 setup.py build_ext --inplace

if [ $? -eq 0 ]; then
    echo "Build successful! Module created:"
    ls -la ai_tracker_body*.so
    echo -e "\nTo use the module:"
    echo "export LD_LIBRARY_PATH=./lib:\$LD_LIBRARY_PATH"
    echo "python3 test_person.py"
else
    echo "Build failed!"
    exit 1
fi