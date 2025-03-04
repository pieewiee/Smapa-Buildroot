#!/bin/bash

setup_shell_environment() {
    local TARGET_DIR="$1"
    
    # Set permissions for bashrc files
    chmod 644 ${TARGET_DIR}/etc/skel/.bashrc
    chmod 644 ${TARGET_DIR}/root/.bashrc

    # Create directory structure
    mkdir -p ${TARGET_DIR}/etc/skel
    mkdir -p ${TARGET_DIR}/root

    # Add bash_profile
    cat > ${TARGET_DIR}/root/.bash_profile << 'EOF'
# Source bashrc if it exists
if [ -f ~/.bashrc ]; then
    source ~/.bashrc
fi
EOF

    chmod 644 ${TARGET_DIR}/root/.bash_profile
}

# Execute the function with the provided target directory
setup_shell_environment "$1"