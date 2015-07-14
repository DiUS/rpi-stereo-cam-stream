# Adding Barometric, Accelerometer, Magnetometer, Gyro Sensors

Adafruit has an excellent [Adafruit 10-DOF IMU Breakout - L3GD20H + LSM303 + BMP180](http://www.adafruit.com/product/1604) breakout board.

### Hardware - connect the sensor board
```
Compute module (J5 header) | Sensor Board
      GPIO 2 (SDA1)        |     SDA
      GPIO 3 (SCL1)        |     SCL
           GND             |     GND
           3V3             |     3Vo
```

### Enable device-tree support
* Copy bcm2708-rpi-cm.dtb to /boot on the Pi.
* Enable device-tree support from raspi-config.
* Reboot and make sure device-tree is enabled. If device-tree is enabled, /proc/device-tree will exist.

To troubleshoot device-tree problem:
```
vcdbg log msg
```

### Enable sensors device-tree overlay
* Copy the below files to /boot/overlays on the Pi.
    * lsm303dlhc_i2c-sensor-overlay.dtb
    * l3gd20_i2c-sensor-overlay.dtb
* Enable I2C from raspi-config.
* Add the following lines to /boot/config.txt
    ```
    dtoverlay=bmp085_i2c-sensor
    dtoverlay=lsm303dlhc_i2c-sensor
    dtoverlay=l3gd20_i2c-sensor
    ```
* For some reason, the kernel does *not* load the gyro driver automatically, so let's add it to /etc/modules.
    ```
    st_gyro_i2c
    ```

### Fix up ST sensors drivers and recompile kernel
* Get the kernel source:
    ```
    git clone https://github.com/raspberrypi/linux.git
    cd linux
    git checkout rpi-3.18.y
    cd -
    git clone git://github.com/raspberrypi/tools.git
    ```
* Apply the patch kernel_3.18.11-fix_st_sensors.patch:
    ```
    patch -p1 -d linux < kernel_3.18.11-fix_st_sensors.patch
    ```
* Set up environment variables:
    ```
    export ARCH=arm
    export CROSS_COMPILE=~/rpi-stereo-cam-stream/tools/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian/bin/arm-linux-gnueabihf-
    export INSTALL_MOD_PATH=~/rpi-stereo-cam-stream/modules
    export KERNEL_SRC=~/rpi-stereo-cam-stream/linux
    ```
* Set up kernel config:
    ```
    cd linux
    make mrproper
    ssh pi@rpi 'zcat /proc/config.gz > /tmp/.config'
    scp pi@rpi:/tmp/.config .
    ```
* Enable IIO and BMP180 driver
    ```
    make menuconfig
    ```
    * Misc devices  --->  BMP085 digital pressure sensor on I2C
    * Industrial I/O support  --->  Accelerometers  --->  STMicroelectronics accelerometers 3-Axis Driver
    * Industrial I/O support  --->  Digital gyroscope sensors  --->  STMicroelectronics gyroscopes 3-Axis Driver
    * Industrial I/O support  --->  Magnetometer sensors  --->  STMicroelectronics magnetometers 3-Axis Driver
* Compile kernel
    ```
    make -j4
    make modules_install
    ```
* Prepare build artifacts
    ```
    cd ..
    rm modules/lib/modules/3.18.*/build modules/lib/modules/3.18.*/source
    tar czf modules.tar.gz modules/
    cd tools/mkimage
    ./mkknlimg ${KERNEL_SRC}/arch/arm/boot/zImage kernel.img
    cd ../..
    ```
* Copy artifacts to Pi.


### Compile device-tree blob (dtb)
* git clone git://git.kernel.org/pub/scm/linux/kernel/git/jdl/dtc
* Compile dtb like so:
    ```
    ./dtc -@ -I dts -O dtb -o lsm303dlhc_i2c-sensor-overlay.dtb lsm303dlhc_i2c-sensor-overlay.dts
    ./dtc -@ -I dts -O dtb -o l3gd20_i2c-sensor-overlay.dtb     l3gd20_i2c-sensor-overlay.dts
    ```

