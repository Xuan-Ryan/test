#!/bin/sh

while :
do
        AOAHID=$(ps |grep uvcclient |grep -v grep)
        if [ "$AOAHID" == "" ]; then
                uvcclient&
		date >> /tmp/uvcclient_restart
        fi
        sleep 1
done

