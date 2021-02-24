#!/bin/sh
. /sbin/config.sh
. /sbin/global.sh

ip=`nvram_get 2860 lan_ipaddr`
nm=`nvram_get 2860 lan_netmask`

interval=10

rm /tmp/timeout 1>/dev/null 2>/dev/null

while :
do
	opmode=`nvram_get 2860 OperationMode`
	if [ "$opmode" = "1" ]; then
		sleep 5
		continue
	fi

	sleep $interval

	if [ "$opmode" != "1" ]; then
		# check connection via conn_status command
		disconnect=`iwpriv apclii0 conn_status|grep ApClii0|grep Disconnect`
		if [ -n "$disconnect" ]; then
			#  disconnect, do connection again
			ifconfig apclii0 down up
			iwpriv apclii0 set ApCliEnable=1
			iwpriv apclii0 set SiteSurvey=1
			sleep 1
			iwpriv apclii0 get_site_survey
			iwpriv apclii0 set ApCliAutoConnect=1
		else
			count=0
			#  connected, don't to do anything
		fi
	fi
done
