#!/bin/sh

#### FOR DEVELOPING ONLY DOES NOT CONTAIN 99kernel INIT SCRIPT TO CONFIG THE KERNEL. ASSUMES YOU ARE DOING A DIRTY FLASH ####

## date format ##
NOW=$(date +"%F")
NOWT=$(date +"%T")

# Number of jobs (usually the number of cores your CPU has (if Hyperthreading count each core as 2))
MAKE="4"

## Set compiler location to compile with linaro cortex a8
echo Setting compiler location...
export ARCH=arm
export CROSS_COMPILE=$HOME/android/system/prebuilt/linux-x86/toolchain/linaro/bin/arm-cortex_a8-linux-gnueabi-

## Build kernel using pyramid_defconfig
make mrproper
make evervolv_glacier_defconfig
make -j$MAKE ARCH=arm
sleep 1

# Post compile tasks
echo Copying compiled kernel and modules to $HOME/KERNEL/
echo and building flashable zip
sleep 1

     mkdir -p $HOME/KERNEL/
     mkdir -p $HOME/KERNEL/out/
     mkdir -p $HOME/KERNEL/out/system/lib/modules/
     mkdir -p $HOME/KERNEL/out/kernel/
     mkdir -p $HOME/KERNEL/out/META-INF/

cp -a $(find . -name *.ko -print |grep -v initramfs) $HOME/KERNEL/out/system/lib/modules/
cp -rf prebuilt-scripts/META-INF/ $HOME/KERNEL/out/
cp -rf prebuilt-scripts/kernel_dir/* $HOME/KERNEL/out/kernel/
cp arch/arm/boot/zImage $HOME/KERNEL/out/kernel/

# build flashable zip

cd $HOME/KERNEL/out/
zip -9 -r $HOME/Chronicglacier-dev-$NOW-$NOWT.zip .
echo Deleting Temp files and folders....
rm -rf $HOME/KERNEL/
echo Build Complete, Check your Home directory for a flashable zip
