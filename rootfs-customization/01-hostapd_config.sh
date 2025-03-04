#!/bin/bash

setup_wlan_hotspot() {
   local TARGET_DIR="$1"
   
   # Create necessary directories
   mkdir -p ${TARGET_DIR}/etc/wlan-configs
   mkdir -p ${TARGET_DIR}/lib/systemd/system
   mkdir -p ${TARGET_DIR}/etc
   
   # Create hostapd network configuration in disabled location
   cat > ${TARGET_DIR}/etc/wlan-configs/hostapd.network << 'EOF'
[Match]
Name=wlan0

[Network]
Address=192.168.72.1/24
DHCPServer=yes
IPForward=ipv4
EOF

   # Create hostapd configuration
   cat > ${TARGET_DIR}/etc/hostapd.conf << 'EOF'
interface=wlan0
driver=nl80211
hw_mode=g
ssid=SMAPA
channel=7
wmm_enabled=0
macaddr_acl=0
auth_algs=1
ignore_broadcast_ssid=0
wpa=2
wpa_passphrase=smapa
wpa_key_mgmt=WPA-PSK
wpa_pairwise=TKIP
rsn_pairwise=CCMP
EOF

   # Create hostapd service file
   cat > ${TARGET_DIR}/lib/systemd/system/hostapd.service << 'EOF'
[Unit]
Description=Hostapd IEEE 802.11 AP
After=network.target

[Service]
Type=forking
ExecStartPre=/sbin/ip link set wlan0 down
ExecStartPre=/bin/sleep 1
ExecStartPre=/sbin/ip link set wlan0 up
ExecStartPre=/bin/sleep 1
ExecStart=/usr/sbin/hostapd /etc/hostapd.conf -P /run/hostapd.pid -B
PIDFile=/run/hostapd.pid

[Install]
WantedBy=multi-user.target
EOF

   # Set proper permissions
   chmod 644 ${TARGET_DIR}/etc/wlan-configs/hostapd.network
   chmod 644 ${TARGET_DIR}/etc/hostapd.conf
   chmod 644 ${TARGET_DIR}/lib/systemd/system/hostapd.service

   # Make sure the service is not enabled by default
   #rm -f ${TARGET_DIR}/etc/systemd/system/multi-user.target.wants/hostapd.service
}

# Execute the function with the provided target directory
setup_wlan_hotspot "$1"