#!/bin/bash
setup_ldconfig() {
    local TARGET_DIR="$1"
    
    # Create ld.so.conf
    cat > ${TARGET_DIR}/etc/ld.so.conf << EOF
/lib
/usr/lib
EOF

    # Make sure ldconfig is executable
    chmod +x ${TARGET_DIR}/sbin/ldconfig

    # Create ldconfig init script
    cat > ${TARGET_DIR}/etc/init.d/S20ldconfig << EOF
#!/bin/sh

case "\$1" in
  start)
    printf "Creating library cache: "
    /sbin/ldconfig
    echo "OK"
    ;;
  *)
    echo "Usage: \$0 {start}"
    exit 1
esac
exit 0
EOF

    chmod +x ${TARGET_DIR}/etc/init.d/S20ldconfig
}

# Execute the function with the provided target directory
setup_ldconfig "$1"