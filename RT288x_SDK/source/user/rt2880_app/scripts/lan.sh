#!/bin/sh
#
# $Id: //WIFI_SOC/MP/SDK_4_3_0_0/RT288x_SDK/source/user/rt2880_app/scripts/lan.sh#1 $
#
# usage: wan.sh
#

. /sbin/config.sh
. /sbin/global.sh

# stop all
killall -q udhcpd 1>/dev/null 2>/dev/null
killall -q lld2d
killall -q igmpproxy
killall -q upnpd
killall -q pppoe-relay
killall -q dnsmasq
rm -rf /var/run/lld2d-*
echo "" > /var/udhcpd.leases

# ip address
ip=`nvram_get 2860 lan_ipaddr`
nm=`nvram_get 2860 lan_netmask`
#ifconfig $lan_if down
ifconfig $lan_if $ip netmask $nm

# hostname
host=`nvram_get 2860 HostName`
if [ "$host" = "" ]; then
	host="j5create"
	nvram_set 2860 HostName j5create
fi
hostname $host
echo "127.0.0.1 localhost.localdomain localhost" > /etc/hosts
echo "$ip $host.j5create.com $host" >> /etc/hosts

# dhcp server
dhcp=`nvram_get 2860 dhcpEnabled`
if [ "$dhcp" = "1" ]; then
	fname="/etc/udhcpd.conf"
	leases="/var/udhcpd.leases"
	killall -q udhcpd
	touch $leases

	start=`nvram_get 2860 dhcpStart`
	end=`nvram_get 2860 dhcpEnd`
	mask=`nvram_get 2860 dhcpMask`
	pd=`nvram_get 2860 dhcpPriDns`
	sd=`nvram_get 2860 dhcpSecDns`
	gw=`nvram_get 2860 dhcpGateway`
	lease=`nvram_get 2860 dhcpLease`
	echo "start $start" > $fname
	echo "end $end" >> $fname
	echo "interface $lan_if" >> $fname
	echo "option subnet $mask" >> $fname
	echo "option dns $pd $sd" >> $fname
	echo "option router $gw" >> $fname
	echo "option lease $lease" >> $fname
	echo "lease_file $leases" >> $fname
	
	udhcpd $fname
fi

