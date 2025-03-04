# SMAPA Image - Smart Panel Device OS

This repository contains the build system for creating the operating system image for the Smart Panel Device based on STM32MP153C. The image integrates TF-A, OP-TEE, U-Boot, Linux, and a root filesystem into a complete SD card image.

## Device Information

The Smart Panel device runs on STM32MP153C hardware and provides the following connection interfaces:

- **USB-C port** providing USB Ethernet/RNDIS Gadget connectivity
- Default network configuration: 192.168.7.1
- Default login: `root` (no password required)

## Flashing the Image

### Prerequisites

You'll need:
- The image file (`sdcard.img.gz`)
- A compatible SD card (8GB or larger recommended)

### Linux

1. Install `bmap-tools`:
   ```
   sudo apt-get install bmap-tools
   ```

2. Flash the image:
   ```
   sudo bmaptool copy sdcard.img.gz /dev/sdX
   ```
   Replace `/dev/sdX` with your SD card device (use `lsblk` to identify it).

### macOS

1. Install `bmap-tools` via Homebrew:
   ```
   brew install bmap-tools
   ```

2. Flash the image:
   ```
   sudo bmaptool copy sdcard.img.gz /dev/diskX
   ```
   Replace `/dev/diskX` with your SD card device (use `diskutil list` to identify it).

### Windows

The simplest way to flash the SD card image on Windows is using Rufus:

1. Download and install [Rufus](https://rufus.ie/)
2. Extract the `sdcard.img.gz` file to get `sdcard.img`
3. Launch Rufus
4. Select your SD card in the device dropdown
5. Click the "SELECT" button and choose the extracted `sdcard.img` file
6. Click "START" to begin flashing
7. Wait for the process to complete before removing the SD card

## Building the Image

### Prerequisites

- Linux build environment (Ubuntu 20.04 or later recommended)
- Required packages: `build-essential gcc g++ gperf bison flex texinfo help2man make libncurses5-dev python3-dev python3-setuptools unzip bzip2 libtool-bin wget curl git`

### Local Build

1. Clone the repository:
   ```
   git clone https://github.com/your-org/smapa-image.git
   cd smapa-image
   ```

2. Build the image:
   ```
   make O=$PWD/build -C buildroot
   ```

3. The resulting image files will be in the `build/images/` directory:
   - `sdcard.img.gz` - Compressed SD card image
   - `sdcard.img.bmap` - Block map file for faster flashing

### Building with GitHub Actions

You can use GitHub Actions to automatically build the image without setting up a local build environment:

1. Fork or clone the repository on GitHub
2. Make your desired changes
3. Push the changes to GitHub (or create a pull request)
4. GitHub Actions will automatically start building the image
5. Once the build is complete, you can download the image from the "Actions" tab:
   - Navigate to the specific workflow run
   - Scroll down to the "Artifacts" section
   - Download the "buildroot-images" artifact which contains the SD card image files

## Customizing the Build

### Understanding the Configuration Structure

The build system is based on Buildroot, with the main configuration in the `build/.config` file. This configuration defines which packages, kernel options, and system settings are included in the build.

Key directories:
- `build/`: Contains the Buildroot configuration and build output
- `rootfs-overlay/`: Contains files that will be copied to the root filesystem
- `rootfs-customization/`: Contains scripts that modify the root filesystem during build

### Modifying the Configuration

To modify the build configuration:

1. Edit the existing configuration with menuconfig:
   ```
   make O=$PWD/build -C buildroot menuconfig
   ```

2. Save your changes and exit menuconfig

3. Rebuild the image:
   ```
   make O=$PWD/build -C buildroot
   ```

### Cleaning the Build

To clean the build directory and start fresh:

1. Clean the entire build:
   ```
   make O=$PWD/build -C buildroot clean
   ```

2. Alternatively, to clean only specific packages:
   ```
   make O=$PWD/build -C buildroot <package>-dirclean
   ```
   Replace `<package>` with the name of the package to clean (e.g., `linux-dirclean` for the kernel)

## Connecting to the Device

1. Insert the flashed SD card into the device and power it on
2. Connect to the device using the USB-C port:
   - The device will appear as a USB Ethernet adapter with IP 192.168.7.1
   - Connect via SSH: `ssh root@192.168.7.1`

## Features and Configuration

### Network Configuration

- **USB Ethernet**: Configured with static IP 192.168.7.1 and DHCP server for connected clients
- **Wi-Fi**: Requires manual configuration

To configure Wi-Fi:
1. Add your network to `/etc/wpa_supplicant/wpa_supplicant-wlan0.conf`:
   ```
   ctrl_interface=/var/run/wpa_supplicant
   network={
       ssid="YourNetworkName"
       psk="YourPassword"
   }
   ```
2. Alternatively, use the `wpa_passphrase` utility:
   ```
   wpa_passphrase YourNetworkName YourPassword >> /etc/wpa_supplicant/wpa_supplicant-wlan0.conf
   ```

### SSH Access

- Root login is enabled by default
- Add your SSH key to `/root/.ssh/authorized_keys` for key-based authentication

### Preinstalled Packages

The image comes with several useful utilities:
- vim, nano (text editors)
- tmux (terminal multiplexer)
- jq (JSON processor)
- openssl (cryptographic library)
- systemd-networkd (network management)
- wpa_supplicant (Wi-Fi configuration)
- SSH server