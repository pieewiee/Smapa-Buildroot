#!/bin/bash

# Get the directory where this script is located
SCRIPT_DIR="$(dirname "$(readlink -f "$0")")"
TARGET_DIR="$1"

echo "$TARGET_DIR"
echo "$SCRIPT_DIR"

# Find all executable .sh files in the directory, excluding the wrapper itself and disabled scripts
for script in $(find "${SCRIPT_DIR}" -maxdepth 1 -type f -name "*.sh" ! -name "$(basename "$0")" ! -name "*disabled*.sh" | sort); do
    if [ -x "$script" ]; then
        echo "Executing $(basename "$script")..."
        bash -x "$script" "$TARGET_DIR" 2>&1
        
        # Check if script executed successfully
        if [ $? -ne 0 ]; then
            echo "Error: $(basename "$script") failed!"
            exit 1
        fi
    else
        echo "Warning: $script not found or not executable"
    fi
done

echo "All customization scripts completed successfully"
exit 0