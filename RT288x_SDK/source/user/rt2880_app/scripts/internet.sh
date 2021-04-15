#!/bin/sh
#
# $Id: //WIFI_SOC/MP/SDK_4_3_0_0/RT288x_SDK/source/user/rt2880_app/scripts/internet.sh#9 $
#
# usage: internet.sh
#

. /sbin/config.sh
. /sbin/global.sh

lan_ip=`nvram_get 2860 lan_ipaddr`
stp_en=`nvram_get 2860 stpEnabled`
nat_en=`nvram_get 2860 natEnabled`
radio_off1=`nvram_get 2860 RadioOff`
wifi_off=`nvram_get 2860 WiFiOff`
ra_Bssidnum=`nvram_get 2860 BssidNum`
if [ "$CONFIG_RTDEV" != "" ]; then
	rai_Bssidnum=`nvram_get rtdev BssidNum`
fi

genDevNode()
{
#Linux2.6 uses udev instead of devfs, we have to create static dev node by myself.
if [ "$CONFIG_HOTPLUG" == "y" -a "$CONFIG_USB" == "y" -o "$CONFIG_MMC" == "y" ]; then
	mounted=`mount | grep mdev | wc -l`
	if [ $mounted -eq 0 ]; then
	mount -t ramfs mdev /dev
	mkdir /dev/pts
	mount -t devpts devpts /dev/pts

#        mknod   /dev/video0      c       81      0
#	mknod	/dev/ppp	 c	 108	 0   $cons
        mknod   /dev/spiS0       c       217     0
        mknod   /dev/i2cM0       c       218     0
        mknod   /dev/mt6605      c       219     0
        mknod   /dev/flash0      c       200     0
        mknod   /dev/swnat0      c       210     0
        mknod   /dev/hwnat0      c       220     0
        mknod   /dev/acl0        c       230     0
        mknod   /dev/ac0         c       240     0
        mknod   /dev/mtr0        c       250     0
        mknod   /dev/nvram       c       251     0
        mknod   /dev/gpio        c       252     0
        mknod   /dev/rdm0        c       253     0
        mknod   /dev/pcm0        c       233     0
        mknod   /dev/i2s0        c       234     0	
        mknod   /dev/cls0        c       235     0
	mknod   /dev/spdif0      c       236     0
	mknod   /dev/vdsp        c       245     0
	mknod   /dev/slic        c       225     0
if [ "$CONFIG_SOUND" = "y" ] || [ "$CONFIG_SOUND" = "m" ]; then
	mknod   /dev/controlC0   c       116     0
	mknod   /dev/pcmC0D0c    c       116     24
	mknod   /dev/pcmC0D0p    c       116     16
fi	

	fi
	echo "# <device regex> <uid>:<gid> <octal permissions> [<@|$|*> <command>]" > /etc/mdev.conf
        echo "# The special characters have the meaning:" >> /etc/mdev.conf
        echo "# @ Run after creating the device." >> /etc/mdev.conf
        echo "# $ Run before removing the device." >> /etc/mdev.conf
        echo "# * Run both after creating and before removing the device." >> /etc/mdev.conf
        echo "sd[a-z][1-9] 0:0 0660 */sbin/automount_boot.sh \$MDEV" >> /etc/mdev.conf
        echo "sd[a-z] 0:0 0660 */sbin/automount_boot.sh \$MDEV" >> /etc/mdev.conf
	echo "mmcblk[0-9]p[0-9] 0:0 0660 */sbin/automount_boot.sh \$MDEV" >> /etc/mdev.conf
	echo "mmcblk[0-9] 0:0 0660 */sbin/automount_boot.sh \$MDEV" >> /etc/mdev.conf
	if [ "$CONFIG_USB_SERIAL" = "y" ] || [ "$CONFIG_USB_SERIAL" = "m" ]; then
		echo "ttyUSB0 0:0 0660 @/sbin/autoconn3G.sh connect" >> /etc/mdev.conf
	fi
	if [ "$CONFIG_BLK_DEV_SR" = "y" ] || [ "$CONFIG_BLK_DEV_SR" = "m" ]; then
		echo "sr0 0:0 0660 @/sbin/autoconn3G.sh connect" >> /etc/mdev.conf
	fi
	if [ "$CONFIG_USB_SERIAL_HSO" = "y" ] || [ "$CONFIG_USB_SERIAL_HSO" = "m" ]; then
		echo "ttyHS0 0:0 0660 @/sbin/autoconn3G.sh connect" >> /etc/mdev.conf
	fi
fi
	mknod /dev/Web_UVC0 c 255 0
}

setMDEV()
{
#Linux2.6 uses udev instead of devfs, we have to create static dev node by myself.
if [ "$CONFIG_HOTPLUG" == "y" -a "$CONFIG_USB" == "y" -o "$CONFIG_MMC" == "y" ]; then
	echo "# <device regex> <uid>:<gid> <octal permissions> [<@|$|*> <command>]" > /etc/mdev.conf
        echo "# The special characters have the meaning:" >> /etc/mdev.conf
        echo "# @ Run after creating the device." >> /etc/mdev.conf
        echo "# $ Run before removing the device." >> /etc/mdev.conf
        echo "# * Run both after creating and before removing the device." >> /etc/mdev.conf
        echo "sd[a-z][1-9] 0:0 0660 */sbin/automount.sh \$MDEV" >> /etc/mdev.conf
        echo "sd[a-z] 0:0 0660 */sbin/automount.sh \$MDEV" >> /etc/mdev.conf
	echo "mmcblk[0-9]p[0-9] 0:0 0660 */sbin/automount.sh \$MDEV" >> /etc/mdev.conf
	echo "mmcblk[0-9] 0:0 0660 */sbin/automount.sh \$MDEV" >> /etc/mdev.conf
	if [ "$CONFIG_USB_SERIAL" = "y" ] || [ "$CONFIG_USB_SERIAL" = "m" ]; then
		echo "ttyUSB0 0:0 0660 @/sbin/autoconn3G.sh connect" >> /etc/mdev.conf
	fi
	if [ "$CONFIG_BLK_DEV_SR" = "y" ] || [ "$CONFIG_BLK_DEV_SR" = "m" ]; then
		echo "sr0 0:0 0660 @/sbin/autoconn3G.sh connect" >> /etc/mdev.conf
	fi
	if [ "$CONFIG_USB_SERIAL_HSO" = "y" ] || [ "$CONFIG_USB_SERIAL_HSO" = "m" ]; then
		echo "ttyHS0 0:0 0660 @/sbin/autoconn3G.sh connect" >> /etc/mdev.conf
	fi

        #enable usb hot-plug feature
        echo "/sbin/mdev" > /proc/sys/kernel/hotplug
fi
}

set_vlan_map()
{
	# vlan priority tag => skb->priority mapping
	vconfig set_ingress_map $1 0 0
	vconfig set_ingress_map $1 1 1
	vconfig set_ingress_map $1 2 2
	vconfig set_ingress_map $1 3 3
	vconfig set_ingress_map $1 4 4
	vconfig set_ingress_map $1 5 5
	vconfig set_ingress_map $1 6 6
	vconfig set_ingress_map $1 7 7

	# skb->priority => vlan priority tag mapping
	vconfig set_egress_map $1 0 0
	vconfig set_egress_map $1 1 1
	vconfig set_egress_map $1 2 2
	vconfig set_egress_map $1 3 3
	vconfig set_egress_map $1 4 4
	vconfig set_egress_map $1 5 5
	vconfig set_egress_map $1 6 6
	vconfig set_egress_map $1 7 7
}

addBr0()
{
	brctl addbr br0

	# configure stp forward delay
	if [ "$wan_if" = "br0" -o "$lan_if" = "br0" ]; then
		stp=`nvram_get 2860 stpEnabled`
		if [ "$stp" = "1" ]; then
			brctl setfd br0 15
			brctl stp br0 1
		else
			brctl setfd br0 1
			brctl stp br0 0
		fi
	fi

}

genSysFiles()
{
	login=`nvram_get 2860 Login`
	pass=`nvram_get 2860 Password`
	if [ "$login" != "" -a "$pass" != "" ]; then
		echo "$login::0:0:Adminstrator:/:/bin/sh" > /etc/passwd
		echo "$login:x:0:$login" > /etc/group
		chpasswd.sh $login $pass
	fi
	#if [ "$CONFIG_PPPOL2TP" == "y" ]; then
	#echo "l2tp 1701/tcp l2f" > /etc/services
	#echo "l2tp 1701/udp l2f" >> /etc/services
	#fi
}

configVIF()
{
	echo "##### configVIF #####"

	ifconfig eth2 0.0.0.0

	#vconfig rem eth2.1
	vconfig rem eth2.2

	if [ "$CONFIG_VLAN_8021Q" == "m" ]; then
		rmmod 8021q
		insmod -q 8021q
	fi
	#vconfig add eth2 1
	#set_vlan_map eth2.1
	vconfig add eth2 2
	set_vlan_map eth2.2
	ifconfig eth2.2 down
	wan_mac=`nvram_get 2860 WAN_MAC_ADDR`
	if [ "$wan_mac" != "FF:FF:FF:FF:FF:FF" ]; then
		ifconfig eth2.2 hw ether $wan_mac
	fi

	#ifconfig eth2.1 0.0.0.0
	ifconfig eth2.2 0.0.0.0
}

#  Added by Tiger for apclient_chkconn.sh
rm /tmp/timeout 2>/dev/null

genDevNode
genSysFiles

echo 2 > proc/sys/vm/overcommit_memory
echo 50 > proc/sys/vm/overcommit_ratio
echo 0 > /proc/sys/net/ipv4/ip_forward

killall -9 watchdog 1>/dev/null 2>&1
if [ "$CONFIG_RALINK_WATCHDOG" = "m" -o "$CONFIG_RALINK_TIMER_WDG" = "m" ]; then
rmmod ralink_wdt 1>/dev/null 2>&1
fi

ifconfig ra0 down 1>/dev/null 2>&1
ifconfig rai0 down 1>/dev/null 2>&1

ralink_init make_wireless_config rt2860
ralink_init make_wireless_config rtdev

# config interface
#ifconfig ra0 0.0.0.0 1>/dev/null 2>&1
#if [ "$radio_off1" = "1" ]; then
#	iwpriv ra0 set RadioOn=0
#fi

ifconfig lo 127.0.0.1

mdev -s
setMDEV

#
# init ip address to all interfaces for different OperationMode:
#   1 = Gateway Mode
#   3 = AP Client
#
if [ "$opmode" = "1" ]; then
	configVIF

	addBr0
	brctl addif br0 eth2
	brctl addif br0 rai0
	ifconfig rai0 up 1>/dev/null 2>&1
	brctl addif br0 rai0

	wan.sh
	lan.sh
	nat.sh

	# light up all of LED for RX, No need for TX
	#gpio l 14 4000 0 1 0 4000
	#gpio l 52 4000 0 1 0 4000

	# set the global ipv6 address for LAN/WAN, enable ipv6 forwarding,
	# enable ecmh(multicast) daemon
elif [ "$opmode" = "3" ]; then
	configVIF

	addBr0
	brctl addif br0 eth2
	brctl addif br0 rai0
	ifconfig rai0 up 1>/dev/null 2>&1
	brctl addif br0 rai0

	wan.sh
	lan.sh
	nat.sh
	APCLI=`nvram_get rtdev apClient`
	if [ "$APCLI" == "1" ]; then
		ifconfig apclii0 up
		#iwpriv apclii0 set ApCliEnable=1
		brctl addif br0 apclii0
	fi
else
	echo "unknown OperationMode: $opmode"
	exit 1
fi

echo 1 > /proc/sys/net/ipv4/ip_forward

# in order to use broadcast IP address in L2 management daemon
if [ "$CONFIG_ICPLUS_PHY" = "y" ]; then
	route add -host 255.255.255.255 dev $wan_if
else
	route add -host 255.255.255.255 dev $lan_if
fi

if [ "$wifi_off" = "1" ]; then
	ifconfig ra0 down 1>/dev/null 2>&1
	reg s b0180000
	reg w 400 0x1080
	reg w 1204 8
	reg w 1004 3
fi

WDGE=`nvram_get 2860 WatchDogEnable`
if [ "$WDGE" = "1" ]; then
	if [ "$CONFIG_RALINK_WATCHDOG" = "m" -o "$CONFIG_RALINK_TIMER_WDG" = "m" ]; then
		insmod -q ralink_wdt
	fi
	wdg.sh
	watchdog
fi

echo 4096 > /proc/sys/net/ipv4/tcp_max_tw_buckets
