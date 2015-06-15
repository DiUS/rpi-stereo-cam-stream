# How to set up wifi roaming and hotplug ethernet

Edit /etc/network/interfaces:
```
auto lo
iface lo inet loopback

# hotplug ethernet
allow-hotplug eth0
iface eth0 inet dhcp

# wifi roaming
allow-hotplug wlan0
iface wlan0 inet manual
  wpa-roam /etc/wpa_supplicant/wpa-roam.conf
iface default inet dhcp
```

Configure roaming:
```
cat > /etc/wpa_supplicant/wpa-roam.conf << EOF
ctrl_interface=/var/run/wpa_supplicant
ctrl_interface_group=0
update_config=1

EOF
chmod 600 /etc/wpa_supplicant/wpa-roam.conf

wpa_passphrase '1st_Priority_SSID' 'Passphrase' >> /etc/wpa_supplicant/wpa-roam.conf
wpa_passphrase '2nd_Priority_SSID' 'Passphrase' >> /etc/wpa_supplicant/wpa-roam.conf
```

