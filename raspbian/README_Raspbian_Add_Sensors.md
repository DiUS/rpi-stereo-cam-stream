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

* Apply the IIO patches:

    ```
    patch -p1 -d linux < kernel_3.18.11-fix_st_sensors.patch
    patch -p1 -d linux < kernel_3.18.11-iio-add_hrtimer_trigger.patch
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

    Device Drivers  --->
    * Misc devices  --->  BMP085 digital pressure sensor on I2C
    * Industrial I/O support  --->  Enable buffer support within IIO
    * Industrial I/O support  --->  Enable triggered sampling support
    * Industrial I/O support  --->  Triggers - standalone  --->  High resolution timer trigger
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
    rm ${INSTALL_MOD_PATH}/lib/modules/*/build ${INSTALL_MOD_PATH}/lib/modules/*/source
    tar czf modules.tar.gz modules/
    cd tools/mkimage
    ./mkknlimg ${KERNEL_SRC}/arch/arm/boot/zImage kernel.img
    cd ../..
    ```

* Copy artifacts to Pi.

* To load the hrtimer trigger automatically at start up, add this to /etc/modules.
    ```
    iio-trig-hrtimer
    ```

---

### How to use IIO sensors ?

* Create a trigger to be used with the IIO sensors. The trigger can be anything like GPIO interrupt, periodic timer, or sysfs. In this case, we use high resolution timer.

    ```
    echo 0 > /sys/bus/iio/devices/iio_hrtimer_trigger/add_trigger
    ```

* Optional: Change the hrtimer trigger frequency. Unit is in nanoseconds. Default is 10 Hz.

    ```
    echo 10000000 > devices/trigger0/set_delay
    cat devices/trigger0/set_delay
    ```

* List all IIO devices and triggers.

    ```
    lsiio -v
    ```

* Enable sampling on all the sensor data.

    ```
    echo 1 > /sys/bus/iio/devices/iio\:device0/scan_elements/in_magn_x_en
    echo 1 > /sys/bus/iio/devices/iio\:device0/scan_elements/in_magn_y_en
    echo 1 > /sys/bus/iio/devices/iio\:device0/scan_elements/in_magn_z_en

    echo 1 > /sys/bus/iio/devices/iio\:device1/scan_elements/in_accel_x_en
    echo 1 > /sys/bus/iio/devices/iio\:device1/scan_elements/in_accel_y_en
    echo 1 > /sys/bus/iio/devices/iio\:device1/scan_elements/in_accel_z_en

    echo 1 > /sys/bus/iio/devices/iio\:device2/scan_elements/in_anglvel_x_en
    echo 1 > /sys/bus/iio/devices/iio\:device2/scan_elements/in_anglvel_y_en
    echo 1 > /sys/bus/iio/devices/iio\:device2/scan_elements/in_anglvel_z_en
    ```

* Read the samples.

    ```
    generic_buffer -n lsm303dlhc_magn -t hrtimertrig0 -c 1000
    generic_buffer -n lsm303dlhc_accel -t hrtimertrig0 -c 1000
    generic_buffer -n l3gd20 -t hrtimertrig0 -c 1000
    ```

---

### How to compile device-tree blob (dtb) ?

* git clone git://git.kernel.org/pub/scm/linux/kernel/git/jdl/dtc
* Compile dtb like so:

    ```
    ./dtc -@ -I dts -O dtb -o lsm303dlhc_i2c-sensor-overlay.dtb lsm303dlhc_i2c-sensor-overlay.dts
    ./dtc -@ -I dts -O dtb -o l3gd20_i2c-sensor-overlay.dtb     l3gd20_i2c-sensor-overlay.dts
    ```

