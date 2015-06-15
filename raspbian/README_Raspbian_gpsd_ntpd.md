# Enabling UART based GPS receiver and getting ntpd to sync from it

The UART based GPS receiver referenced in this guide is the [PA6H (MTK3339) GPS module][http://www.adafruit.com/products/790]. There is a [breakout module][http://www.adafruit.com/product/746] version for development.
The built-in antenna is quite weak and may have difficulty getting a GPS lock. I highly recommend getting an [active antenna][http://www.adafruit.com/product/960] and the [adapter][http://www.adafruit.com/product/851] to ease indoor testing.

### disable UART from raspi-config
```
raspi-config
```

### Hardware - connect the GPS module
```
Compute module (J5 header) | GPS Module
   GPIO 14 (UART TxD)      |     RX
   GPIO 15 (UART RxD)      |     TX
           GND             |    GND
         3.3 VDC           |    VIN
         GPIO 18           |    PPS
```

### Install GPS tools
```
apt-get install gpsd gpsd-clients python-gps
apt-get install pps-tools
```

### Configure gpsd
```
dpkg-reconfigure gpsd
```

A dialog will appear with some questions that should be answered as follows:

Start gpsd automatically on boot? Yes
Device the GPS receiver is attached to: /dev/ttyAMA0
Should gpsd handle attached USB GPS receivers automatically? No
Options to gpsd: -b -n

Explanation of the two options:
-b - Broken-device-safety, aka read-only mode. This option prevents gpsd from writing anything to the device. In this case it is used to prevent it from shortening the pulse length of the LVC model from its default of 200 ms to a mere 40 ms.
-n - Forces the clock to be updated even though no clients are active. By default, gpsd stops polling the GPS device when clients are no longer connected, and ntpd is not recognized as a client.

Afterwards, these settings are saved in /etc/default/gpsd and the daemon is started automatically.

### Test gpsd
`cgps -s`
`gpsmon`

### Enable PPS input
Non-device-tree: Add "bcm2708.pps_gpio_pin=18" to /boot/cmdline.txt.
Device-tree: Add "dtoverlay=pps-gpio,gpiopin=18" to /boot/config.txt.

Add "pps-gpio" to /etc/modules.

### Testing PPS input
Reboot and check PPS input.
```
# dmesg | grep pps
[    0.140136] bcm2708: GPIO 18 setup as pps-gpio device
[    8.680029] pps_core: LinuxPPS API ver. 1 registered
[    8.682001] pps_core: Software ver. 5.3.6 - Copyright 2005-2007 Rodolfo Giometti <giometti@linux.it>
[    8.696375] pps pps0: new PPS source pps-gpio.18
[    8.698377] pps pps0: Registered IRQ 188 as PPS source
# ppstest /dev/pps0
trying PPS source "/dev/pps0"
found PPS source "/dev/pps0"
ok, found 1 source(s), now start fetching data...

```

### Sync NTP with PPS
Build ntp with ATOM PPS clock.

The ntp package from the Raspbian distribution does not support the "ATOM" (PPS) reflock. You'll have to recompile it.
```
wget http://archive.ntp.org/ntp4/ntp-4.2.8p2.tar.gz
tar xzf ntp-4.2.8p2.tar.gz
cd ntp-4.2.8p2/
./configure --enable-ATOM
make -j2
```

Add the below lines to /etc/ntp.conf.
```
# pps-gpio on /dev/pps0
server 127.127.22.0 minpoll 4 maxpoll 4
fudge 127.127.22.0 refid PPS
fudge 127.127.22.0 flag3 1  # enable kernel PLL/FLL clock discipline

# gpsd shared memory clock
server 127.127.28.0 minpoll 4 maxpoll 4 prefer  # PPS requires at least one preferred peer
fudge 127.127.28.0 refid GPS
fudge 127.127.28.0 time1 +0.130  # coarse processing delay offset
```

Remove ntp-servers from reuest in /etc/dhcp/dhclient.conf.

Remove cached NTP config.
`rm var/lib/ntp/ntp.conf.dhcp`

Reboot and check NTP
```
# ntpq -p -crv

```
