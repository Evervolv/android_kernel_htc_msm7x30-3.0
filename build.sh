make ARCH=arm CROSS_COMPILE=~/android/cm10/prebuilts/gcc/linux-x86/arm/arm-eabi-4.6/bin/arm-eabi- clean;
make ARCH=arm CROSS_COMPILE=~/android/cm10/prebuilts/gcc/linux-x86/arm/arm-eabi-4.6/bin/arm-eabi- glacier_deconfig;
make ARCH=arm CROSS_COMPILE=~/android/cm10/prebuilts/gcc/linux-x86/arm/arm-eabi-4.6/bin/arm-eabi- -j4;
mv arch/arm/boot/zImage ~/android/zImage;
mv drivers/net/wireless/bcmdhd/bcmdhd.ko ~/android/bcmdhd.ko;
