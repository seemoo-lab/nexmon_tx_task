# Nexmon Scheduled Transmission

This patch allows you to inject Wi-Fi frames and schedule their transmission on Broadcom Wi-Fi Chips.  

Currently supported:  
WiFi Chip   | Firmware Version  | Used in
----------- | ----------------- | --------------------
bcm4366c0   | 10.10.122.20      | Asus RT-AC86U


## Getting Started

To compile the source code, you are required to first clone the original `nexmon` repository that contains the C-based patching framework for Wi-Fi firmwares. Then you clone this repository as one of the sub-projects in the corresponding patches sub-directory. This allows you to build and compile all the firmware patches required to inject and schedule frame transmissions. The following guides you through the required procedure.

### Asus RT-AC86U (bcm4366c0)
The following steps, tested under Ubuntu 14.04.1 LTS, will get you started:  
#### First time setup
1. Install some dependencies: `sudo apt-get install git gawk qpdf flex bison`
2. **Only necessary for x86_64 systems**, install i386 libs: 
    ```
    sudo dpkg --add-architecture i386
    sudo apt update
    sudo apt install libc6:i386 libncurses5:i386 libstdc++6:i386
    ```
3. Clone the `nexmon` base repository:  
    ```
    git clone https://github.com/seemoo-lab/nexmon.git
    ```
    _Commit `4131b84` or newer is required for compatibility._  
4. Navigate to the cloned `nexmon` directory and set a couple of environment variables.  
    ```
    cd nexmon
    source setup_env.sh
    ```
5. Run `make` to extract ucode, templateram and flashpatches from the original firmwares.
6. Navigate to the patches directory of the `bcm4366c0` chip firmware version `10.10.122.20` and clone this repository:  
    ```
    cd patches/bcm4366c0/10_10_122_20/
    git clone https://github.com/seemoo-lab/nexmon_tx_task.git
    ```
7. Enter the created subdirectory and compile the firmware patch.
    ```
    cd nexmon_tx_task
    make
    ```
If you can perform the above steps without errors you should be able to compile the patch successfully.  
#### Install and load patched firmware
_We assume SSH is enabled on port 22 on the router, the default user name `admin`, and that you are connected to the router over LAN. Otherwise edit the `install-firmware` target in the `Makefile` according to your needs. Loading the modified firmware will reset the Wi-Fi chip and therefore drop all current Wi-Fi connections to the router(access point)._  
  
Load build environment variables, navigate to the patch directory, compile and install the patched firmware:  
```
cd nexmon
source setup_env.sh
cd patches/bcm4366c0/10_10_122_20/nexmon_tx_task
make install-firmware REMOTEADDR=<address of your rt-ac86u>
```
This first copies a modified version of the `dhd` kernel object to the router at `/jffs/` and then unloads the current `dhd` module and instead loads the modified one.
#### Build and install `nexutil`
The provided `tx_task.sh` util script can be used to operate the patched firmware. It depends on `nexutil` that comes with the `nexmon` repository. Perform the following steps to build and install `nexutil` on the Asus RT-AC86U:
1. Clone the `aarch64` toolchain from Asuswrt-Merlin toolchain repository:  
    ```
    git clone https://github.com/RMerl/am-toolchains.git
    ```
2. Set the compile environment:
    ```
    export AMCC=$(pwd)/am-toolchains/brcm-arm-hnd/crosstools-aarch64-gcc-5.3-linux-4.1-glibc-2.22-binutils-2.25/usr/bin/aarch64-buildroot-linux-gnu-
    export LD_LIBRARY_PATH=$(pwd)/am-toolchains/brcm-arm-hnd/crosstools-aarch64-gcc-5.3-linux-4.1-glibc-2.22-binutils-2.25/usr/lib
    ```
3. Go to the `nexmon` repository and build `nexutil` with the previously set environment:
    ```
    cd nexmon

    cd utilities/libnexio
    ${AMCC}gcc -c libnexio.c -o libnexio.o -DBUILD_ON_RPI
    ${AMCC}ar rcs libnexio.a libnexio.o

    cd ../nexutil
    echo "typedef uint32_t uint;" > types.h
    sed -i 's/argp-extern/argp/' nexutil.c
    ${AMCC}gcc -static -o nexutil nexutil.c bcmwifi_channels.c b64-encode.c b64-decode.c -DBUILD_ON_RPI -DVERSION=0 -I. -I../libnexio -I../../patches/include -L../libnexio/ -lnexio
    ```
4. Copy the `nexutil` binary to the Asus RT-AC86U router and make it executable:  
   _Again we assume SSH is enabled on port 22 on the router, the default user name `admin`, and that you are connected to the router._  
    ```
    scp nexutil admin@<address of your rt-ac86u>:/jffs/nexutil
    ssh admin@<address of your rt-ac86u> "/bin/chmod +x /jffs/nexutil"
    ```

#### Configure the Wi-Fi interface
After loading the customized `dhd` driver, both Wi-Fi interfaces (`eth5` for 2.4 GHz, `eth6` for 5 Ghz band) should be down. To use them again, they need to be configure and started. This might be done by setting a locale, channel, and bringing the interface into the up state.  
E.g. for tuning to the 80 MHz wide channel 157 from 5735 to 5815 MHz with 20 MHz control channel from 5775 to 5795 MHz do the following:  
_Again we assume SSH is enabled on port 22 on the router, the default user name `admin`, and that you are connected to the router._  
```
ssh admin@<address of your rt-ac86u> "/usr/sbin/wl -i eth6 country US"
ssh admin@<address of your rt-ac86u> "/usr/sbin/wl -i eth6 chanspec 157/80"
ssh admin@<address of your rt-ac86u> "/usr/sbin/wl -i eth6 up"
```
  
## Inject and schedule transmissions
_Read [Getting Started](#getting-started) first._  
This patch adds four IOCTLs that can be used to initialize (`429`), start (`430`), stop (`431`), and free (`432`) a scheduled transmission task. In order to perform those IOCTls you might use `nexutil` directly on the host device of the Wi-Fi chip:
```
# initialize (see ioctl.c case 429 for details on the expected parameters)
nexutil -I<interface> -s429 -l<14 + frame_length> -b -v<parameters as base64 string>
# start
nexutil -I<interface> -s430
# stop
nexutil -I<interface> -s431
# free
nexutil -I<interface> -s432
```  
### `tx_task.sh`
_Currently specific to the Asus RT-AC86U, but can be adapted for different environments by modifying a couple of variables._    
To facilitate the process of operating with the patched firmware, we provide an example util script named `tx_task.sh`. It can be installed and operated as follows:  
_We assume SSH is enabled on port 22 on the router, the default user name `admin`, and that you are connected to the router._  
```
# install
cd nexmon/patches/bcm4366c0/10_10_122_20/nexmon_tx_task
make install-util REMOTEADDR=<address of your rt-ac86u>
```
Above copies `utils/tx_task.sh` to the router at `/jffs/` and sets execution rights to it. Afterwards, the following commands can be used to perform IOCTLs with the parameters configured in the script:  
_We assume you are connected to your router via SSH._
```
# initialize
/jffs/tx_task.sh <interface> init
# start
/jffs/tx_task.sh <interface> start
# stop
/jffs/tx_task.sh <interface> stop
# free
/jffs/tx_task.sh <interface> deinit
```
To start a task it must be initialized first. A stopped task can be restarted without initalization using start. Only one task can be initalized at once, therefore, an existing task is automatically cleared when a second one gets initialized. A running task is stopped when freed.  

#### Default configuration
In its current state, this will configure a transmission of a data frame roughly every 500 ms over a bandwidth of 20 MHz with VHT MCS 0 modulation/coding using 1 spatial stream. The default data frame has the following contents:  
```
IEEE 802.11 Data, Flags: ......F.C
    Type/Subtype: Data (0x0020)
    Frame Control Field: 0x0802
        .... ..00 = Version: 0
        .... 10.. = Type: Data frame (2)
        0000 .... = Subtype: 0
        Flags: 0x02
    .000 0000 0000 0000 = Duration: 0 microseconds
    Receiver address: 00:11:22:33:44:55
    Transmitter address: 00:11:22:33:44:66
    Destination address: 00:11:22:33:44:55
    Source address: 00:11:22:33:44:77
    BSS Id: 00:11:22:33:44:66
    STA address: 00:11:22:33:44:55
    .... .... .... 0000 = Fragment number: 0
    0000 0000 0000 .... = Sequence number: 0
Data (4 bytes)
    Data: 12345678
```  
Duration and sequence number are automatically set by the Wi-Fi chip, for repeating transmissions the sequence number increases. Furthermore, a 4 byte frame check sequence is automatically appended by the Wi-Fi chip.
  
#### Adapt the script
The util script (`utils/tx_task.sh`) features a `# user settings` section that can be used to modify the scheduled transmissions.
- You can set `spatial_mode` to either `0`, `1`, or `255`. This is useful to forcefully disable STBC by setting it to `0`.  
- `periodic` controls wether multiple transmissions (`1`) or just a single transmission (`0`) shall be performed.
- The delay before the first and between consecutive transmissions can be set using `tx_delay` in milliseconds.
- A number for periodic transmissions shall be given with `repetitions`, where `-1` means infinite.
- A VHT modulation coding scheme index can be set with `mcs`, together with the number of spatial streams to use in `spatial_streams`.  
  _For more control over the coding settings check out `nexmon/patches/include/rates.h`. Or use the [brcm-ratespec](https://github.com/jlinktu/brcm-ratespec) tool to create a compatible ratespec._
- The bandwidth over wich the frame might be transmitted can be set with `bandwidth` to either `1`, `2`, or `3`, corresponding to 20, 40, or 80 MHz.  
  _Note that you need to tune the Wi-Fi chip at a channel that has at least the expected bandwidth._
- `frame_bytes` and `frame_length` let you control the frame that shall be transmitted. `frame_length` shall be set to the length of bytes given in `frame_bytes`.
  
# Contact
[Jakob Link](https://www.seemoo.tu-darmstadt.de/team/jlink/) <<jlink@seemoo.tu-darmstadt.de>>
  
# Powered By
## Secure Mobile Networking Lab (SEEMOO)
![SEEMOO](gfx/logo_seemoo.png)
## Multi-Mechanisms Adaptation for the Future Internet (MAKI)
![MAKI](gfx/logo_maki.png)
## Technische Universität Darmstadt
![TU Darmstadt](gfx/logo_tud.png)
  
# Reference the `nexmon` project
Any use of this project which results in an academic publication or other publication which includes a bibliography should include a citation to the Nexmon project:
```
@electronic{nexmon:project,
	author = {Schulz, Matthias and Wegemer, Daniel and Hollick, Matthias},
	title = {Nexmon: The C-based Firmware Patching Framework},
	url = {https://nexmon.org},
	year = {2017}
}
```
