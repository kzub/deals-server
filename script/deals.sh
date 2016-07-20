#!/bin/bash
LOGFILE=/tmp/deals-server.log
APPFILE=/home/zubkov/deals-server/bin/deals-server


if [[ $1 = "start" ]]; then

	if [[ -z $2 ]]; then
		echo "server start count neededexample: deals.sh start 0 7"
		exit
	fi

	if [[ -z $3 ]]; then
		echo "server end count needed. example: deals.sh start 0 7"
		exit
	fi

	PORT=$((5000+$2))

	for i in `seq $2 $3`;
	do
		RUN="chpst -o 8000 -P -u zubkov $APPFILE $PORT"
		echo $RUN
		# $RUN >> $LOGFILE 2>&1 &
		PORT=$((1+$PORT))
	done

	echo "ok"
	exit
fi

if [[ $1 = "stop" ]]; then
 killall deals-server
 echo "stopped"
 exit
fi

echo "no params specified"