from setuptools import setup, Extension
import os
import sys

current_dir = os.path.dirname(os.path.abspath(__file__))
lib_path = os.path.join(current_dir, "lib")
include_path = os.path.join(current_dir, "include")

# 只链接核心库，避免架构不兼容
lib_files = [
    'ai_tracker_body',
    'device_manager',
    'perception',
    'opencv_core',
    'opencv_highgui',
    'opencv_imgproc',
    'opencv_videoio',
]

ext_modules = [
    Extension(
        'ai_tracker_body',
        sources=['tracker_final.cpp'],
        include_dirs=[
            include_path,
            os.path.join(include_path, "opencv_4.2.0/include"),
            '/usr/include/python3.10',
        ],
        library_dirs=[lib_path],
        libraries=lib_files,
        runtime_library_dirs=[lib_path],
        language='c++',
        extra_compile_args=[
            '-std=c++11',
            '-O3',
            '-fPIC',
            '-Wall',
        ],
    ),
]

setup(
    name='ai_tracker_body',
    version='1.0.0',
    author='Tracker Developer',
    description='Python interface for object tracker',
    ext_modules=ext_modules,
    python_requires='>=3.6',
)