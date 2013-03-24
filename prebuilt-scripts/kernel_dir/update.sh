#!/sbin/sh
if [ -e "/system/etc/init.d/99sultan-kernel" ];
then
	mount /data
	if [ ! -e "/data/local/sultan-kernel.sh" ]; then
		grep -E "^(SWEEP2WAKE|BADASS|TUN|ZRAM)" /system/etc/init.d/99sultan-kernel > /data/local/sultan-kernel.sh
	fi
	umount /data
fi

