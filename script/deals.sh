#!/bin/bash
LOGFILE=/tmp/deals-server.log
APPFILE=`pwd`"/../bin/deals-server"

if [[ $1 = "start" ]]; then
	PORT=5000
	for i in `seq 1 $2`;
	do
		RUN="chpst -o 8000 -P -u zubkov $APPFILE $PORT >> $LOGFILE&"
		echo $RUN
		$($RUN)
		PORT=$((1+$PORT))
	done 
fi

if [[ $1 = "stop" ]]; then
 killall deals-server
fi