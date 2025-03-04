# smapa-image

Operating System Image for the Smart Panel Device

The image is built using buildroot with scripts (and possibly patches) from Bootlin's buildroot-external-st br2-external tree. It combines TF-A, OP-TEE, U-Boot, Linux and a root filesystem into an SD card image; this is just what buildroot does. Bootlin's br2-external tree currently requires the use of Bootlin's buildroot fork which is in the process of being upstreamed, see the [buildroot-external-st docs](https://github.com/bootlin/buildroot-external-st/blob/st/2024.02.3/docs/internals.md#changes-compared-to-upstream-buildroot). Although we do not use buildroot-external-st as true br2-external tree anymore, we still use Bootlin's buildroot fork for historic reasons.

**Note:** This image is not extensively tested yet. To be safe, you may want to use the Yocto-based image from the [Jenkins build](https://jenkins.apps.sele.iml.fraunhofer.de/job/OpenST-STM32MP1-smapav4/). Specifically, download the file [FlashLayout_sdcard_stm32mp153c-ic-trusted.raw.xz](https://jenkins.apps.sele.iml.fraunhofer.de/job/OpenST-STM32MP1-smapav4/lastSuccessfulBuild/artifact/FlashLayout_sdcard_stm32mp153c-ic-trusted.raw.xz).

## Getting Started

1. Download the sdcard image (`sdcard.img.gz`) and the bmap file (`sdcard.img.bmap`) from the build pipeline. To build the image locally, set up all buildroot requirements and run the step shown in `.gitlab-ci.yml`.
2. Flash the image using a bmap-aware flashing tool, e.g. [`bmap-rs`](https://crates.io/crates/bmap-rs): `bmap-rs copy sdcard.img.gz <path-to-sdcard>`

**Note:** For security reasons, the `/etc/wpa_supplicant/wpa_supplicant-wlan0.conf` does not contain any configured networks by default so if Wi-Fi shall be used, this has to be configured first. See the `Features` section below on how to do so.

## Repository Layout

We use a buildroot [out-of-tree build](http://buildroot.org/downloads/manual/manual.html#_building_out_of_tree). Like this, we can store the buildroot configuration file(s) in this repository while buildroot itself is fetched externally. Additionally, we use buildroot's [`<pkg>_OVERRIDE_SRCDIR` mechanism](https://buildroot.org/downloads/manual/manual.html#_advanced_usage) (Section 8.13.6 "Using Buildroot during development") with a `local.mk` file to use our forks of TF-A, OP-TEE, U-Boot and Linux. Both the buildroot and buildroot-external-st sources as well as TF-A, OP-TEE, U-Boot and Linux are managed and fetched using git submodules.

This setup, especially using `local.mk` which is supposed to be used for "development", may be unusual but it removes the necessity to manage patches and out-of-tree device-tree source files. Buildroot itself is only able to download TF-A, OP-TEE, U-Boot and Linux from upstream. Since we might also want to use ST forks and since we definitely want to add custom device-tree sources, this would normally result in patching the sources and adding the device-tree sources externally. This is possible but adding all this in custom forks of the repositories seems more manageable.

In summary, the `build` directory contains the `.config` and `local.mk` file, the buildroot, buildroot-external-st, TF-A, OP-TEE, U-Boot and Linux repositories are managed using git submodules. The build itself happens in the `build` directory using a buildroot out-of-tree build.

### Submodule Initialization Workaround

Unfortunately, we have observed alarmingly slow Fraunhofer cc-asp Gitlab performance (around 1MB/s speed). For example, cloning the `linux` submodule which is around 5GB big would take nearly 2 hours then. Luckily, there is a nice workaround to speed up the initial submodule clone. Afterwards, all git operations are fast enough because the vast majority of the repositories is already present locally:
1. Run `git submodule init`. This copies the submodule information from `.gitmodules` to `.git/config`.
2. Replace the submodule urls by the urls of the repos which we forked our repos from. At the time of writing, these are (ordered by repository size):
    - https://github.com/STMicroelectronics/linux.git
    - https://github.com/bootlin/buildroot.git
    - https://github.com/STMicroelectronics/u-boot
    - https://github.com/STMicroelectronics/arm-trusted-firmware
    - https://github.com/STMicroelectronics/optee_os.git
    - https://github.com/bootlin/buildroot-external-st.git
3. Run `git submodule update`. This will fail because the referenced commits are missing in the upstream repositories.
4. Run `git submodule sync`. This restores the submodule urls in `.git/config` and sets them as remote urls in the submodule directories.
5. Run `git submodule update` again. This time, it should succeed and bring the repo into the desired state.

For the gitlab-runner build server, the submodule initialization has already been done. gitlab-runner reuses previous repositories of the same project, thus the pipeline setup is reasonably fast.

## Features

Many features in the image are configurable by adjusting the corresponding files in the image appropriately. These changes may be done in various ways, for example:
1. In the `rootfs-overlay` before building the image. This requires a rebuild and possibly some knowledge about buildroot. 
2. In the sdcard image mounted via a loop device.
3. In a mounted sdcard after flashing the sdcard.
4. In the running system. If the configuration changes affect network or ssh server setup, this may require a serial connection.

### Version Info
The git version (`git describe`) of the currently running image is dumped into `/smapa-image-version-info`.

### Network + mDNS
- `systemd-networkd` for network configuration
- USB CDC ECM + RNDIS for Ethernet over USB with a static `192.168.7.1/24` address for `usb0`
- DHCP server for the USB Ethernet connection
- Wi-Fi authentication for `wlan0` using `wpa_supplicant`
- **Note:** No network configurations by default in `/etc/wpa_supplicant/wpa_supplicant-wlan0.conf` because this would require Wi-Fi credentials in the repository
- DHCP for `wlan0` by default
- Multicast DNS enabled for `wlan0`
- Default hostname set to `smapa` in buildroot config, so the default mDNS name is `smapa.local`

#### Configuration
- Add your network configuration(s) to `/etc/wpa_supplicant/wpa_supplicant-wlan0.conf`, e.g. `wpa_passphrase <SSID> <PSK>  >>  <mountpoint>/etc/wpa_supplicant/wpa_supplicant-wlan0.conf`; make sure not to remove the line `ctrl_interface=/var/run/wpa_supplicant` for `wpa_cli` to work from the `smapa-mp1-software`
- Adjust `/etc/systemd/network/wlan0.network`, e.g. to set a static IP address
- Create/Change `/etc/hostname` to overwrite the default hostname, e.g. if multiple devices are used in the same network

### SSH Server
- Root login allowed
- Some keys are authorized by the supplied `/root/.ssh/authorized_keys` in the rootfs overlay

#### Configuration
- Add your key to `/root/.ssh/authorized_keys` to authorize yourself to login with your keypair

### Ash Shell
By default, the ash shell is used. The image contains a basic ash profile in `/etc/profile` which sets a simple prompt and adds some aliases. To see what aliases are set, just have a look at the file.

#### Configuration
To configure ash to your preferences, adjust `/etc/profile`.

### Installed Applications
- vim
- nano
- tmux
- jq
- openssl

#### Configuration
To add more applications, the buildroot config has to be changed. Normally, this is not done in the rootfs-overlay.

### M4 Firmware
TODO: 
For historic reasons, a working `m4-firmware` is automatically included in the image. It should be removed in the future so do not rely on it.
