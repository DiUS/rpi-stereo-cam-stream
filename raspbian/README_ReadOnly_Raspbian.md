# Converting Raspbian to read-only filesystem

It is highly recommended to update Raspbian to the latest version before proceeding with read-only filesystem conversion.

### Install unionfs
```
apt-get install unionfs-fuse
```

### Disable filesystem checks, disable swap, mount root as read-only
Add "fastboot noswap ro" to /boot/cmdline.txt.
Example:
```
dwc_otg.lpm_enable=0 console=ttyAMA0,115200 kgdboc=ttyAMA0,115200 console=tty1 root=/dev/mmcblk0p2 rootfstype=ext4 rootwait fastboot noswap ro
```

### Mount root and boot partitions as read-only
Add "ro" flag to /etc/fstab.
Example:
```
proc            /proc           proc    defaults                0       0
/dev/mmcblk0p1  /boot           vfat    ro,defaults             0       0
/dev/mmcblk0p2  /               ext4    ro,noatime,defaults     0       0
/dev/mmcblk0p3  /storage        ext4    defaults,noatime        0       0
mount_unionfs   /etc            fuse    defaults                0       0
mount_unionfs   /var            fuse    defaults                0       0
```

### Create script to remount root with read write permissions
```
cat > /usr/local/bin/remountrootrw << EOF
#!/bin/sh
mount -o remount,rw /
EOF
```

### Create script to mount unionfs
```
cat > /usr/local/bin/mount_unionfs << EOF
#!/bin/sh
[ -z "$1" ] && exit 1 || DIR=$1
ROOT_MOUNT=$(grep -v "^#" /etc/fstab | awk '$2=="/" {print substr($4,1,2)}')
if [ "$ROOT_MOUNT" != "ro" ]; then
	/bin/mount --bind ${DIR}_org ${DIR}
else
	/bin/mount -t tmpfs ramdisk ${DIR}_rw
	/usr/bin/unionfs-fuse -o cow,allow_other,suid,dev,nonempty ${DIR}_rw=RW:${DIR}_org=RO ${DIR}
fi
EOF
```

### Create script to re-enable read-write environment
```
cat > /usr/local/bin/rwchroot.sh << EOF
#!/bin/bash

# remount root rw
mount -o remount,rw /

# prapare target paths
mkdir -p /chroot
mkdir -p /chroot/{bin,boot,dev,etc,home,lib,opt,proc,root,run,sbin,sys,tmp,usr,var}

# mount special filesystems
mount -t proc proc /chroot/proc
mount --rbind /sys /chroot/sys
mount --rbind /dev /chroot/dev

# bind rw directories
for f in {etc,var}; do mount --rbind /${f}_org /chroot/$f; done

# bind remaining directories
for f in {bin,boot,home,lib,opt,root,run,sbin,tmp,usr}; do mount --rbind /$f /chroot/$f; done

# chroot
echo "Note: /boot is still mounted read-only, remount to read-write if needed."
echo -e "\e[33mYou are now in read-write chroot. Use CTRL+D when done to exit chroot and mount read-only again.\e[39m"
chroot /chroot /usr/bin/env PS1="(rw) \u@\h:\w\$ " /bin/bash --noprofile -l

# unmount mounts
for f in /chroot/{bin,boot,dev,etc,home,lib,opt,proc,root,run,sbin,sys,tmp,usr,var}; do
umount -l $f
done

sleep 1

# remount read-only again
echo -e "\e[32mChroot left, re-mounting read-only again.\e[39m"
mount -o remount,ro /
EOF
```

### Make the above scripts executable
```
chmod +x /usr/local/bin/remountrootrw /usr/local/bin/mount_unionfs /usr/local/bin/rwchroot.sh
```

### Prepare read write directories for unionfs
```
cp -al /etc /etc_org
mv /var /var_org
mkdir /etc_rw
mkdir /var /var_rw
```

### Reboot and check
Reboot and make sure root and boot are mounted as read-only.
```
# mount
/dev/root on / type ext4 (ro,noatime,data=ordered)
/dev/mmcblk0p1 on /boot type vfat (ro,relatime,fmask=0022,dmask=0022,codepage=437,iocharset=ascii,shortname=mixed,errors=remount-ro)
ramdisk on /etc_rw type tmpfs (rw,relatime)
unionfs-fuse on /etc type fuse.unionfs-fuse (rw,relatime,user_id=0,group_id=0,default_permissions,allow_other)
ramdisk on /var_rw type tmpfs (rw,relatime)
unionfs-fuse on /var type fuse.unionfs-fuse (rw,relatime,user_id=0,group_id=0,default_permissions,allow_other)
```

### Clean up logs
Remove logs that are not needed anymore to save disk space.
```
remountrootrw
cd /var_org/log
for f in $(find . -name \*log*); do > $f; done
for f in $(find . -name wtmp\*); do > $f; done
for f in $(find . -name messages\*); do > $f; done
for f in $(find . -name debug\*); do > $f; done
for f in $(find . -name dmesg\*); do > $f; done
```

### Remove docs
Tell Raspbian *not* to install docs.
```
remountrootrw
cat > /etc_org/dpkg/dpkg.cfg.d/01_nodoc << EOF
path-exclude /usr/share/doc/*
path-exclude /usr/share/man/*
path-exclude /usr/share/groff/*
path-exclude /usr/share/info/*
path-exclude /usr/share/lintian/*
EOF
```

Remove locale, docs, and user manual to save more disk space.
```
remountrootrw
rm -rf /usr/share/doc/* /usr/share/man/* /usr/share/groff/* /usr/share/info/* /var_org/cache/man/* /usr/share/lintian/*
find /usr/share/locale/* -maxdepth 0 -type d |grep -v en |xargs rm -rf
```

### Use fake-hwclock to set default system time
```
rwchroot.sh
apt-get install fake-hwclock
date -s "2015-06-15 12:00"
fake-hwclock save
```

To adjust the time in fake hwclock, edit /etc/fake-hwclock.data.

Add "service fake-hwclock start" to /etc/rc.local to restore a default system time.


### Improve write performance
```
tune2fs -o journal_data_writeback /dev/mmcblk0p3
tune2fs -O ^has_journal /dev/mmcblk0p3
e2fsck -f /dev/mmcblk0p3
```
Edit /etc/fstab:
```
/dev/mmcblk0p3  /storage        ext4    defaults,noatime,nodiratime,data=writeback,errors=remount-ro  0       0
```
