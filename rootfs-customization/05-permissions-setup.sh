#!/bin/bash

setup_permissions() {
    local TARGET_DIR="$1"


    
    chmod 755 ${TARGET_DIR}/usr/sbin/ldconfig
    chmod 755 ${TARGET_DIR}/usr/sbin/update-ca-certificates
  
    chmod 755 ${TARGET_DIR}/usr/bin/dpkg
    chmod 755 ${TARGET_DIR}/usr/bin/dpkg-architecture
    chmod 755 ${TARGET_DIR}/usr/bin/dpkg-buildapi
    chmod 755 ${TARGET_DIR}/usr/bin/dpkg-buildflags
    chmod 755 ${TARGET_DIR}/usr/bin/dpkg-buildpackage
    chmod 755 ${TARGET_DIR}/usr/bin/dpkg-buildtree
    chmod 755 ${TARGET_DIR}/usr/bin/dpkg-checkbuilddeps
    chmod 755 ${TARGET_DIR}/usr/bin/dpkg-deb
    chmod 755 ${TARGET_DIR}/usr/bin/dpkg-distaddfile
    chmod 755 ${TARGET_DIR}/usr/bin/dpkg-divert
    chmod 755 ${TARGET_DIR}/usr/bin/dpkg-genbuildinfo
    chmod 755 ${TARGET_DIR}/usr/bin/dpkg-genchanges
    chmod 755 ${TARGET_DIR}/usr/bin/dpkg-gencontrol
    chmod 755 ${TARGET_DIR}/usr/bin/dpkg-gensymbols
    chmod 755 ${TARGET_DIR}/usr/bin/dpkg-maintscript-helper
    chmod 755 ${TARGET_DIR}/usr/bin/dpkg-mergechangelogs
    chmod 755 ${TARGET_DIR}/usr/bin/dpkg-name
    chmod 755 ${TARGET_DIR}/usr/bin/dpkg-parsechangelog
    chmod 755 ${TARGET_DIR}/usr/bin/dpkg-query
    chmod 755 ${TARGET_DIR}/usr/bin/dpkg-realpath
    chmod 755 ${TARGET_DIR}/usr/bin/dpkg-scanpackages
    chmod 755 ${TARGET_DIR}/usr/bin/dpkg-scansources
    chmod 755 ${TARGET_DIR}/usr/bin/dpkg-shlibdeps
    chmod 755 ${TARGET_DIR}/usr/bin/dpkg-source
    chmod 755 ${TARGET_DIR}/usr/bin/dpkg-split
    chmod 755 ${TARGET_DIR}/usr/bin/dpkg-statoverride
    chmod 755 ${TARGET_DIR}/usr/bin/dpkg-trigger
    chmod 755 ${TARGET_DIR}/usr/bin/dpkg-vendor

    chmod 755 ${TARGET_DIR}/usr/sbin/dpkg-fsys-usrunmess
    chmod 755 ${TARGET_DIR}/usr/sbin/dpkg-preconfigure
    chmod 755 ${TARGET_DIR}/usr/sbin/dpkg-reconfigure

}

setup_permissions $1