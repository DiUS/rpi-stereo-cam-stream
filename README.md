# rpi-stereo-cam-stream
Raspberry Pi Stereo Camera Live Stream

Stereo camera mode requires Raspberry Pi Compute Module as it exposes both CSI ports.

### Set up Stereo Camera:
![Image of Camera connection](https://github.com/jasaw/rpi-stereo-cam-stream/blob/master/docs/CMAIO-Cam-Adapter.jpg)

Connect jumper wires:
```
CD1_SDA  (J6 pin 37)  <--->  GPIO0  (J5 pin 1)
CD1_SCL  (J6 pin 39)  <--->  GPIO1  (J5 pin 3)
CAM1_IO1 (J6 pin 41)  <--->  GPIO4  (J5 pin 9)
CAM1_IO0 (J6 pin 43)  <--->  GPIO5  (J5 pin 11)

CD0_SDA  (J6 pin 45)  <--->  GPIO28 (J6 pin 1)
CD0_SCL  (J6 pin 47)  <--->  GPIO29 (J6 pin 3)
CAM0_IO1 (J6 pin 49)  <--->  GPIO30 (J6 pin 5)
CAM0_IO0 (J6 pin 51)  <--->  GPIO31 (J6 pin 7)
```

Copy dual-camera device tree blob:
`scp dt-blob-dualcam-pin4pin5.dtb root@rpi:/boot/dt-blob.bin`

### Set up Compute Module:
Follow instructions here: http://www.element14.com/community/community/raspberry-pi/raspberry-pi-compute-module/blog/2014/06/26/raspberry-pi-compute-module--getting-started

### Install raspi-config if not already installed:
http://archive.raspberrypi.org/debian/pool/main/r/raspi-config/raspi-config_20150131-4_all.deb

### Enable camera:
`raspi-config`

### Install raspicam tools:
http://archive.raspberrypi.org/debian/pool/main/r/raspberrypi-firmware/raspberrypi-bootloader_1.20150421-1_armhf.deb
http://archive.raspberrypi.org/debian/pool/main/r/raspberrypi-firmware/libraspberrypi0_1.20150421-1_armhf.deb
http://archive.raspberrypi.org/debian/pool/main/r/raspberrypi-firmware/libraspberrypi-bin_1.20150421-1_armhf.deb

### Mount tmpfs as ramdisk:
Edit file /etc/default/tmpfs
`RAMTMP=yes`

### Install node:
```
wget http://nodejs.org/dist/v0.10.17/node-v0.10.17-linux-arm-pi.tar.gz
tar xzf node-v0.10.17-linux-arm-pi.tar.gz --directory /opt
```

### Running the stream server on the Pi:
`/opt/node-v0.10.17-linux-arm-pi/bin/node index.js`

The web server listens on port 3000. Point your web browser to http://rpi:3000
