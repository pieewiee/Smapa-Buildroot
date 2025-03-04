#!/bin/bash

TARGET_DIR="$1"

# Create LSB release file
echo -e "DISTRIB_ID=Linux\nDISTRIB_RELEASE=1\nDISTRIB_CODENAME=linux\nDISTRIB_DESCRIPTION=\"Linux 1\"" > "${TARGET_DIR}/etc/lsb-release"