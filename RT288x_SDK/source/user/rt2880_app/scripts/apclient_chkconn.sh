#!/bin/sh
. /sbin/config.sh
. /sbin/global.sh

ip=`nvram_get 2860 lan_ipaddr`
nm=`nvram_get 2860 lan_netmask`

interval=10
count=0

rm /tmp/timeout 1>/dev/null 2>/dev/null
opmode=`nvram_get 2860 OperationMode`
if [ "$opmode" = "1" ]; then
	exit
fi

connected="0"
disconnect=`iwpriv apclii0 conn_status|grep ApClii0|grep Disconnect`
if [ -n "$disconnect" ]; then
	# turn off green LED
	gpio l 52 0 4000 0 1 4000
	# light up red LED
	gpio l 14 4000 0 1 0 4000
	iwpriv apclii0 set SiteSurvey=1 1>/dev/null
	sleep 1
	iwpriv apclii0 get_site_survey 1>/dev/null
	iwpriv apclii0 set ApCliAutoConnect=1 1>/dev/null
else
	connected="1"
	# light up green LED
	gpio l 52 4000 0 1 0 4000
	# light up red LED
	gpio l 14 4000 0 1 0 4000
fi

while :
do
	sleep $interval

	if [ ! -e /tmp/doing_wps ]; then
		# check connection via conn_status command
		disconnect=`iwpriv apclii0 conn_status|grep ApClii0|grep Disconnect`
		if [ -n "$disconnect" ]; then
			interval=2
			if [ "$connected" = "1" ]; then
				connected="0"
				# turn off green LED
				gpio l 52 0 4000 0 1 4000
				# light up red LED
				gpio l 14 4000 0 1 0 4000
				count=0
			fi
			#  disconnect, do connection again
			ssid=`nvram_get rtdev ApCliSsid`
			if [ -n "$ssid" ]; then
				count=$((count+1))
				if [ "$count" -gt "10" ]; then
					#  waiting for 20's and do auto-connect again
					iwpriv apclii0 set ApCliAutoConnect=1 1>/dev/null
					ifconfig apclii0 down 1>/dev/null
					ifconfig rai0 down up 1>/dev/null
					ifconfig apclii0 up 1>/dev/null
					count=0
				fi
			fi
		else
			interval=10
			count=0
			#  store the SSID from AP
			#  ex: ApClii0 Connected AP : 00:05:1B:00:01:02   SSID:UVC-79366D
			CUR_SSID=`iwpriv apclii0 conn_status|awk -F ' ' '{print $6}'|awk -F ':' '{print $2}'`
			if [ -n "$CUR_SSID" ]; then
				ORG_SSID=`nvram_get rtdev ApCliSsid`
				SSID=`iwpriv apclii0 stat|grep SSID`
				SSID=`echo $SSID|awk -F '=' '{print $2}'`
				SSID=`echo $SSID|xargs`
				AUTHTYPE=`iwpriv apclii0 stat|grep AuthType`
				AUTHTYPE=`echo $AUTHTYPE|awk -F '=' '{print $2}'`
				AUTHTYPE=`echo $AUTHTYPE|xargs`
				ENCRYPTYPE=`iwpriv apclii0 stat|grep EncrypType`
				ENCRYPTYPE=`echo $ENCRYPTYPE|awk -F '=' '{print $2}'`
				ENCRYPTYPE=`echo $ENCRYPTYPE|xargs`
				KEY=`iwpriv apclii0 stat|grep "Key "`
				KEY=`echo $KEY|awk -F '=' '{print $2}'`
				KEY=`echo $KEY|xargs`
				if [ -n "$SSID" ] && [ "$SSID" != "$ORG_SSID" ]; then
					nvram_set rtdev ApCliSsid "$SSID"

					if [ -n `echo $AUTHTYPE|grep WPA2` ]; then
						nvram_set rtdev ApCliAuthMode WPA2PSK
						#nvram_set rtdev ApCliWPAPSK "$KEY"
					elif [ -n `echo $AUTHTYPE|grep WPA` ]; then
						nvram_set rtdev ApCliAuthMode WPAPSK
						#nvram_set rtdev ApCliWPAPSK "$KEY"
					elif [ -n `echo $AUTHTYPE|grep SHARED` ]; then
						nvram_set rtdev ApCliAuthMode SHARED
						#nvram_set rtdev ApCliKey1Str "$KEY"
					elif [ -n `echo $AUTHTYPE|grep OPEN` ]; then
						nvram_set rtdev ApCliAuthMode OPEN
						#nvram_set rtdev ApCliKey1Str "$KEY"
					else
						nvram_set rtdev ApCliAuthMode WPA2PSK
						#nvram_set rtdev ApCliWPAPSK "$KEY"
					fi

					if [ -n `echo $ENCRYPTYPE|grep AES` ]; then
						nvram_set rtdev ApCliEncrypType AES
					elif [ -n `echo $ENCRYPTYPE|grep TKIP` ]; then
						nvram_set rtdev ApCliEncrypType AES
					elif [ -n `echo $ENCRYPTYPE|grep WEP` ]; then
						nvram_set rtdev ApCliEncrypType WEP
					elif [ -n `echo $ENCRYPTYPE|grep NONE` ]; then
						nvram_set rtdev ApCliEncrypType NONE
					else
						nvram_set rtdev ApCliEncrypType AES
					fi
				fi
			fi
			if [ "$connected" = "0" ]; then
				connected="1"
				# light up green LED
				gpio l 52 4000 0 1 0 4000
				if [ -e /tmp/uvcclient_disconnect ]; then
					# light up red LED
					gpio l 14 4000 0 1 0 4000
				fi
			fi
		fi
	else
		interval=10
	fi
done
