# rpi-stereo-cam-stream
Raspberry Pi Stereo Camera Live Stream

### Install raspi-config if not already installed:
http://archive.raspberrypi.org/debian/pool/main/r/raspi-config/raspi-config_20150131-4_all.deb

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
