#!/bin/bash

setup_ca_certificates_folder() {
    local TARGET_DIR="$1"


    mkdir -p ${TARGET_DIR}/usr/share/ca-certificates
    chmod 755 ${TARGET_DIR}/usr/share/ca-certificates


    mkdir -p ${TARGET_DIR}/usr/share/ca-certificates/mozilla
    chmod 755 ${TARGET_DIR}/usr/share/ca-certificates/mozilla


    
    mkdir -p ${TARGET_DIR}/usr/local/share/ca-certificates
    chmod 755 ${TARGET_DIR}/usr/local/share/ca-certificates


    chmod 644 ${TARGET_DIR}/etc/ca-certificates.conf

}

setup_ca_certificates_folder $1