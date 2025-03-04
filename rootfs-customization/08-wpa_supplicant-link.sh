#!/bin/bash

setup_wpa_supplicant() {
    local TARGET_DIR="$1"
    local LINK_PATH="${TARGET_DIR}/etc/wpa_supplicant/wpa_supplicant-wlan0.conf"
    local TARGET_PATH="${TARGET_DIR}/etc/wpa_supplicant/wpa_supplicant.conf"

    # Remove existing link if it exists
    if [ -L "${LINK_PATH}" ]; then
        rm "${LINK_PATH}"
    fi

    # Create the symbolic link
    #ln -s "${TARGET_PATH}" "${LINK_PATH}"
}

# Execute the function with the provided target directory
setup_wpa_supplicant "$1"