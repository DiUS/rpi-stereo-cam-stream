# How to enable watchdog timer

Add "bcm2708_wdog" to /etc/modules.

Install watchdog software:
`apt-get install watchdog`

Edit /etc/watchdog.conf:
```
Uncomment the line watchdog-device = /dev/watchdog
Uncomment the line with max-load-1 = 24
```

Enable  the watchdog to start at boot and start it now:
```
insserv watchdog
service watchdog start
```

Enable reboot on kernel panic:
`Add "kernel.panic = 10" to /etc/sysctl.conf.`
