#!/bin/sh
if [ ! -e .type ]; then
	echo "Hello, You didn't select RX or TX yet, please make your choice."
	read -p "Compile it as RX(Y/n):" ans
	ans=`echo $ans|tr '[:lower:]' '[:upper:]'`
	if [ -z $ans ]; then
		echo 'You choice to compile as RX'
		echo "R" > .type
	elif [ $ans = 'YES' -o $ans = 'Y' ]; then
		echo 'You choice to compile as RX'
		echo "R" > .type
	else
		echo 'You choice to compile as TX'
		echo "T" > .type
	fi
	echo "******************************************"
	echo "Please execute 'make' again"
	echo "******************************************"
	exit 1
fi

__TYPE=`cat .type`

make oldconfig_config
if [ $__TYPE = 'R' ]; then
	cp mctconfig/linux-config linux-2.6.36.x/.config
	cp mctconfig/userconfig config/.config
	cp mctconfig/busybox-config user/busybox/.config
	cp mctconfig/mct-config.in config/config.in 
	cp mctconfig/uClibc-config uClibc-0.9.33.2/.config
else
	cp mctconfig/linux-config-tx linux-2.6.36.x/.config
	cp mctconfig/userconfig-tx config/.config
	cp mctconfig/busybox-config user/busybox/.config
	cp mctconfig/mct-config.in config/config.in 
	cp mctconfig/uClibc-config uClibc-0.9.33.2/.config
fi

./mctconfig/patch/do-patch.sh
