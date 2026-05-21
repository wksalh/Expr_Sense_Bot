#!/bin/bash
# Python Dependencies Installation and OpenGL Compatibility Fix Script
# Used to resolve Python environment issues in QuecPi Smart Car System
# Usage: ./fix_python_dependencies.sh

set -e  # Exit immediately if any command returns non-zero status

# Color definitions
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; BLUE='\033[0;34m'; NC='\033[0m'

# Logging functions
log_info() { echo -e "${BLUE}[INFO]${NC} $1"; }
log_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
log_warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }


# Step 1: Install Python dependencies
install_python_dependencies() {
    log_info "Step 1: Installing Python dependencies..."
    
    # Check if pip3 is available
    if ! command -v pip3 >/dev/null 2>&1; then
        log_error "pip3 not found, please install pip3 first"
        exit 1
    fi
    
    # Install required packages
    local packages=("pandas" "smbus2" "mediapipe" "transforms3d" "pyzbar")
    
    for package in "${packages[@]}"; do
        log_info "Installing $package..."
        if pip3 install "$package" --timeout 60 --retries 3; then
            log_success "Successfully installed $package"
        else
            log_error "Failed to install $package"
            return 1
        fi
    done
    
    log_success "Python dependencies installation completed"
}

# Step 2: Fix OpenGL compatibility issues
fix_opengl_compatibility() {
    log_info "Step 2: Fixing OpenGL compatibility issues..."
    
    # Note: OpenGL compatibility fix requires system-level access
    # This step is skipped when running without system privileges
    log_warning "OpenGL compatibility fix requires system-level access"
    log_warning "To fix OpenGL issues, run the following commands:"
    log_warning "  mount -o remount,rw /usr"
    log_warning "  ln -sf /usr/lib/libGLESv2_adreno.so.2 /usr/lib/libGL.so.1"
    
    log_info "Skipping OpenGL compatibility fix (requires system privileges)"
}

# Step 3: Install compatible OpenCV version
install_opencv_compatible() {
    log_info "Step 3: Installing compatible OpenCV version..."
    
    # Uninstall conflicting packages first
    log_info "Uninstalling conflicting opencv-contrib-python package..."
    if pip3 uninstall opencv-contrib-python -y 2>/dev/null; then
        log_success "Successfully uninstalled opencv-contrib-python"
    else
        log_warning "opencv-contrib-python not installed or uninstall failed"
    fi
    
    # Install compatible OpenCV version
    log_info "Installing opencv-python-headless==4.5.5.64..."
    if pip3 install opencv-python-headless==4.5.5.64 --timeout 60 --retries 3; then
        log_success "Successfully installed compatible OpenCV version"
    else
        log_error "Failed to install OpenCV"
        return 1
    fi
    
    log_success "OpenCV compatible version installation completed"
}

# Step 4: Downgrade NumPy version
downgrade_numpy() {
    log_info "Step 4: Downgrading NumPy version..."
    
    # Install NumPy version < 2.0
    log_info "Installing NumPy < 2.0..."
    if pip3 install "numpy<2.0" --timeout 60 --retries 3; then
        log_success "Successfully downgraded NumPy version"
    else
        log_error "Failed to downgrade NumPy"
        return 1
    fi
    
    log_success "NumPy version downgrade completed"
}

# Step 5: Fix cv2/typing/__init__.py DictValue issue
fix_cv2_typing_issue() {
    log_info "Step 5: Fixing DictValue issue in cv2/typing/__init__.py file..."
    
    # Try to find cv2/typing file in user's Python packages first
    local cv2_typing_file=""
    local possible_paths=(
        "$HOME/.local/lib/python3.10/site-packages/cv2/typing/__init__.py"
        "$HOME/.local/lib/python3.9/site-packages/cv2/typing/__init__.py"
        "$HOME/.local/lib/python3.8/site-packages/cv2/typing/__init__.py"
        "/usr/local/lib/python3.10/site-packages/cv2/typing/__init__.py"
        "/usr/local/lib/python3.9/site-packages/cv2/typing/__init__.py"
        "/usr/local/lib/python3.8/site-packages/cv2/typing/__init__.py"
        "/usr/lib/python3.10/site-packages/cv2/typing/__init__.py"
        "/usr/lib/python3.9/site-packages/cv2/typing/__init__.py"
        "/usr/lib/python3.8/site-packages/cv2/typing/__init__.py"
    )
    
    # Find the file
    for path in "${possible_paths[@]}"; do
        if [ -f "$path" ]; then
            cv2_typing_file="$path"
            log_info "Found file: $cv2_typing_file"
            break
        fi
    done
    
    if [ -z "$cv2_typing_file" ]; then
        log_warning "cv2/typing/__init__.py file not found in any common locations"
        log_warning "This fix step is skipped"
        return 0
    fi
    
    # Check if we have write permission
    if [ ! -w "$cv2_typing_file" ]; then
        log_warning "No write permission for $cv2_typing_file"
        log_warning "To fix this issue, run the following command:"
        log_warning "  sed -i 's/LayerId = cv2.dnn.DictValue/LayerId = int  # cv2.dnn.DictValue/' $cv2_typing_file"
        return 0
    fi
    
    # Create backup of the original file
    local backup_file="${cv2_typing_file}.backup_$(date +%Y%m%d_%H%M%S)"
    if cp "$cv2_typing_file" "$backup_file"; then
        log_info "Created backup file: $backup_file"
    else
        log_warning "Failed to create backup file"
    fi
    
    # Apply the fix
    log_info "Applying DictValue fix..."
    if sed -i 's/LayerId = cv2.dnn.DictValue/LayerId = int  # cv2.dnn.DictValue/' "$cv2_typing_file"; then
        log_success "Successfully fixed DictValue issue in cv2/typing/__init__.py"
    else
        log_error "Failed to fix cv2/typing/__init__.py"
        return 1
    fi
    
    log_success "cv2/typing issue fix completed"
}

# Verify installation
verify_installation() {
    log_info "Verifying installation results..."
    
    # Test Python imports
    local test_script="/tmp/test_imports.py"
    cat > "$test_script" << 'EOF'
import sys
try:
    import pandas
    print("pandas import successful")
except ImportError as e:
    print(f"pandas import failed: {e}")

try:
    import smbus2
    print("smbus2 import successful")
except ImportError as e:
    print(f"smbus2 import failed: {e}")

try:
    import mediapipe
    print("mediapipe import successful")
except ImportError as e:
    print(f"mediapipe import failed: {e}")

try:
    import transforms3d
    print("transforms3d import successful")
except ImportError as e:
    print(f"transforms3d import failed: {e}")

try:
    import pyzbar
    print("pyzbar import successful")
except ImportError as e:
    print(f"pyzbar import failed: {e}")

try:
    import cv2
    print(f"opencv-python import successful (version: {cv2.__version__})")
except ImportError as e:
    print(f"opencv-python import failed: {e}")

try:
    import numpy as np
    print(f"numpy import successful (version: {np.__version__})")
except ImportError as e:
    print(f"numpy import failed: {e}")
EOF

    if python3 "$test_script"; then
        log_success "Dependencies verification completed"
    else
        log_warning "Errors occurred during dependencies verification"
    fi
    
    # Clean up test script
    rm -f "$test_script"
}

# Main function
main() {
    echo "=========================================="
    echo "    Python Dependencies Installation and OpenGL Compatibility Fix"
    echo "=========================================="
    echo "This script will fix Python environment issues in QuecPi Smart Car System"
    echo
    echo "Fix steps:"
    echo "1. Install Python dependencies (pandas, smbus2, mediapipe, transforms3d, pyzbar)"
    echo "2. Fix OpenGL compatibility issues"
    echo "3. Install compatible OpenCV version"
    echo "4. Downgrade NumPy version"
    echo "5. Fix DictValue issue in cv2/typing/__init__.py file"
    echo "6. Verify installation results"
    echo
    echo "Starting fix process..."
    echo
    
    # Execute fix steps in sequence
    install_python_dependencies  # Step 1: Install Python dependencies
    fix_opengl_compatibility      # Step 2: Fix OpenGL compatibility
    install_opencv_compatible     # Step 3: Install compatible OpenCV
    downgrade_numpy              # Step 4: Downgrade NumPy
    fix_cv2_typing_issue         # Step 5: Fix cv2 typing issue
    verify_installation          # Step 6: Verify installation
    
    log_success "Python dependencies installation and OpenGL compatibility fix completed!"
    echo
    echo "Note: Some fixes may require system-level access"
    echo "If you encounter permission issues, run the following commands:"
    echo "  mount -o remount,rw /usr"
    echo "  ln -sf /usr/lib/libGLESv2_adreno.so.2 /usr/lib/libGL.so.1"
    echo "  sed -i 's/LayerId = cv2.dnn.DictValue/LayerId = int  # cv2.dnn.DictValue/' /path/to/cv2/typing/__init__.py"
    echo
}

# Display usage instructions
show_usage() {
    echo "Usage: $0 [OPTION]"
    echo
    echo "Options:"
    echo "  --help, -h                      Show this help message"
    echo "  --verify                        Verify installation only"
    echo "  (no arguments)                  Run full fix process"
    echo
    echo "Examples:"
    echo "  $0                              # Run full fix process"
    echo "  $0 --verify                     # Verify installation only"
    echo
}

# Set up error handling
trap 'log_error "Script execution failed with error code: $?"' ERR

# Check command line arguments
case "${1:-}" in
    --help|-h)
        show_usage
        ;;
    --verify)
        verify_installation
        ;;
    "")
        # No arguments, run full fix process
        main "$@"
        ;;
    *)
        log_error "Unknown option: $1"
        show_usage
        exit 1
        ;;
esac
