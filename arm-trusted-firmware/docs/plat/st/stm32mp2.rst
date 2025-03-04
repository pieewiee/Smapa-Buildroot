STM32MP2
========

STM32MP2 is a microprocessor designed by STMicroelectronics
based on Arm Cortex-A35.

More information can be found on `STM32MP2 Series`_ page.

For TF-A common configuration of STM32 MPUs, please check
:ref:`STM32 MPUs` page or `Github STM32 MPUs`_.

STM32MP2 Versions
-----------------

The STM32MP25 series is available in 4 different lines which are pin-to-pin compatible:

- STM32MP257: Dual Cortex-A35 cores, Cortex-M33 core - 3x Ethernet (2+1 switch) - 3x CAN FD – H264 - 3D GPU – AI / NN - LVDS
- STM32MP255: Dual Cortex-A35 cores, Cortex-M33 core - 2x Ethernet – 3x CAN FD - H264 - 3D GPU – AI / NN - LVDS
- STM32MP253: Dual Cortex-A35 cores, Cortex-M33 core - 2x Ethernet – 3x CAN FD - LVDS
- STM32MP251: Single Cortex-A35 core, Cortex-M33 core - 1x Ethernet

Each line comes with a security option (cryptography & secure boot) and a Cortex-A frequency option:

- A      Basic + Cortex-A35 @ 1GHz
- C      Secure Boot + HW Crypto + Cortex-A35 @ 1GHz
- D      Basic + Cortex-A35 @ 1.5GHz
- F      Secure Boot + HW Crypto + Cortex-A35 @ 1.5GHz

The `STM32MP2 part number codification`_ page gives more information about part numbers.

Memory mapping
--------------

::

    0x00000000 +-----------------+
               |                 |   ROM
    0x00020000 +-----------------+
               |                 |
               |       ...       |
               |                 |
    0x0E000000 +-----------------+ \
               |       BL31      | |
               +-----------------+ |
               |       ...       | |
    0x0E012000 +-----------------+ |
               |     BL2 DTB     | | Embedded SRAM
    0x0E016000 +-----------------+ |
               |       BL2       | |
    0x0E040000 +-----------------+ /
               |                 |
               |       ...       |
               |                 |
    0x40000000 +-----------------+
               |                 |
               |                 |   Devices
               |                 |
    0x80000000 +-----------------+ \
               |                 | |
               |                 | | Non-secure RAM (DDR)
               |                 | |
    0xFFFFFFFF +-----------------+ /


Build Instructions
------------------

STM32MP2x specific flags
~~~~~~~~~~~~~~~~~~~~~~~~

Dedicated STM32MP2 build flags:

- | ``STM32MP_DDR_FIP_IO_STORAGE``: to store DDR firmware in FIP.
  | Default: 1
- | ``STM32MP_M33_TDCID``: Enable this flag if Cortex-A35 does not have the Trusted Domain Compartment ID (owned by Cortex-M33)
  | Default: 0
- | ``STM32MP25``: to select STM32MP25 variant configuration.
  | Default: 1

To compile the correct DDR driver, one flag must be set among:

- | ``STM32MP_DDR3_TYPE``: to compile DDR3 driver and DT.
  | Default: 0
- | ``STM32MP_DDR4_TYPE``: to compile DDR4 driver and DT.
  | Default: 0
- | ``STM32MP_LPDDR4_TYPE``: to compile LpDDR4 driver and DT.
  | Default: 0


Boot with FIP
~~~~~~~~~~~~~
You need to build BL2, BL31, BL32 (OP-TEE) and BL33 (U-Boot) and retrieve
DDR PHY firmware before building FIP binary.

U-Boot
______

.. code:: bash

    cd <u-boot_directory>
    make stm32mp25_defconfig
    make DEVICE_TREE=stm32mp257f-ev1 all

OP-TEE
______

.. code:: bash

    cd <optee_directory>
    make CROSS_COMPILE64=aarch64-none-elf- CROSS_COMPILE32=arm-none-eabi-
        ARCH=arm PLATFORM=stm32mp2 \
        CFG_EMBED_DTB_SOURCE_FILE=stm32mp257f-ev1.dts

DDR PHY firmware
________________
DDR PHY firmware files may not be delivered inside TF-A repository, especially
if you build directly from trustedfirmware.org repository. It then needs to be
retrieved from `STMicroelectronics DDR PHY github`_.

You can either clone the repository to the default directory:

.. code:: bash

    git clone https://github.com/STMicroelectronics/stm32-ddr-phy-binary.git drivers/st/ddr/phy/firmware/bin

Or clone it somewhere else, and add ``STM32MP_DDR_FW_PATH=`` in your make command
line when building FIP.

TF-A BL2
________
To build TF-A BL2 with its STM32 header for SD-card boot:

.. code:: bash

    make CROSS_COMPILE=aarch64-none-elf- PLAT=stm32mp2 \
        STM32MP_DDR4_TYPE=1 SPD=opteed \
        DTB_FILE_NAME=stm32mp257f-ev1.dtb STM32MP_SDMMC=1

For other boot devices, you have to replace STM32MP_SDMMC in the previous command
with the desired device flag.


FIP
___

.. code:: bash

    make CROSS_COMPILE=aarch64-none-elf- PLAT=stm32mp2 \
        STM32MP_DDR4_TYPE=1 SPD=opteed \
        DTB_FILE_NAME=stm32mp257f-ev1.dtb \
        BL33=<u-boot_directory>/u-boot-nodtb.bin \
        BL33_CFG=<u-boot_directory>/u-boot.dtb \
        BL32=<optee_directory>/tee-header_v2.bin \
        BL32_EXTRA1=<optee_directory>/tee-pager_v2.bin
        fip

Trusted Boot Board
__________________

.. code:: shell

    tools/cert_create/cert_create -n --rot-key build/stm32mp2/release/rot_key.pem \
        --tfw-nvctr 0 \
        --ntfw-nvctr 0 \
        --key-alg ecdsa --hash-alg sha256 \
        --trusted-key-cert build/stm32mp2/release/trusted_key.crt \
        --tos-fw <optee_directory>/tee-header_v2.bin \
        --tos-fw-extra1 <optee_directory>/tee-pager_v2.bin \
        --tos-fw-cert build/stm32mp2/release/tos_fw_content.crt \
        --tos-fw-key-cert build/stm32mp2/release/tos_fw_key.crt \
        --nt-fw <u-boot_directory>/u-boot-nodtb.bin \
        --nt-fw-cert build/stm32mp2/release/nt_fw_content.crt \
        --nt-fw-key-cert build/stm32mp2/release/nt_fw_key.crt \
        --hw-config <u-boot_directory>/u-boot.dtb \
        --fw-config build/stm32mp2/release/fdts/stm32mp257f-ev1-fw-config.dtb \
        --stm32mp-cfg-cert build/stm32mp2/release/stm32mp_cfg_cert.crt \
        --soc-fw-config build/stm32mp2/release/fdts/stm32mp257f-ev1-bl31.dtb \
        --soc-fw build/stm32mp2/release/bl31.bin \
        --soc-fw-cert build/stm32mp2/release/soc_fw_content.crt \
        --soc-fw-key-cert build/stm32mp2/release/soc_fw_key.crt \
        --ddr-fw drivers/st/ddr/phy/firmware/bin/stm32mp2/ddr4_pmu_train.bin

    tools/fiptool/fiptool create \
        --tos-fw <optee_directory>/tee-header_v2.bin \
        --tos-fw-extra1 <optee_directory>/tee-pager_v2.bin \
        --nt-fw <u-boot_directory>/u-boot-nodtb.bin \
        --hw-config <u-boot_directory>/u-boot.dtb \
        --fw-config build/stm32mp2/release/fdts/stm32mp257f-ev1-fw-config.dtb \
        --soc-fw-config build/stm32mp2/release/fdts/stm32mp257f-ev1-bl31.dtb \
        --tos-fw-cert build/stm32mp2/release/tos_fw_content.crt \
        --tos-fw-key-cert build/stm32mp2/release/tos_fw_key.crt \
        --nt-fw-cert build/stm32mp2/release/nt_fw_content.crt \
        --nt-fw-key-cert build/stm32mp2/release/nt_fw_key.crt \
        --stm32mp-cfg-cert build/stm32mp2/release/stm32mp_cfg_cert.crt \
        --ddr-fw drivers/st/ddr/phy/firmware/bin/stm32mp2/ddr4_pmu_train.bin \
        --trusted-key-cert build/stm32mp2/release/trusted_key.crt \
        --soc-fw-cert build/stm32mp2/release/soc_fw_content.crt \
        --soc-fw-key-cert build/stm32mp2/release/soc_fw_key.crt \
        --soc-fw build/stm32mp2/release/bl31.bin \
        build/stm32mp2/release/fip.bin

.. _STM32MP2 Series: https://www.st.com/en/microcontrollers-microprocessors/stm32mp2-series.html
.. _STM32MP2 part number codification: https://wiki.st.com/stm32mpu/wiki/STM32MP25_microprocessor#Part_number_codification
.. _Github STM32 MPUs: https://github.com/STMicroelectronics/arm-trusted-firmware/tree/HEAD/docs/plat/st/stm32mpus.rst
.. _STMicroelectronics DDR PHY github: https://github.com/STMicroelectronics/stm32-ddr-phy-binary

*Copyright (c) 2023-2024, STMicroelectronics - All Rights Reserved*
