#!/bin/sh
. /sbin/config.sh
. /sbin/global.sh

lanip=`web 2860 nvram lan_ipaddr`
lanmask=`web 2860 nvram lan_netmask`

set_static_ip()
{
	ifconfig br0 $lanip netmask $lanmask
}

if [ "$opmode" != "1" ]; then
	ifconfig br0 $lanip netmask $lanmask
fi
