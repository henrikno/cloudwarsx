#!/bin/bash

EXT=`echo "$1"|awk -F . '{print $NF}'`
NAME=`basename $1 .$EXT`

if [ -z "$4" ]; then
	echo "Usage: $0 <level> <rounds> <ai-bot> <ai-bot>"
	exit 1
fi

LEVEL=$1
ROUNDS=$2
AI1=$3
AI2=$4
CLOUDWARSX="./cloudwarsx -m timelimit -1 ai -2 ai -x 640 -y 480 -n -d -l $LEVEL"

echo "round,winner" 

for((i = 1; i <= $ROUNDS; i++)); do
	$CLOUDWARSX > cloudwarsx.log &
	sleep 2

	turn=$(($i % 2))
	if [ $turn -eq 1 ]; then
		$AI1 > ai1.log &
		sleep 1
		$AI2 > ai2.log &
	else
		$AI2 > ai2.log &
		sleep 1
		$AI1 > ai1.log &
	fi

	while true; do
		if grep -q "Game finish" cloudwarsx.log; then
			break
		fi
	done

	WINNER=$(grep "wins" cloudwarsx.log | awk '{print $1}')
	if [ "$WINNER" != "" ]; then
		echo "$i,$WINNER"
	else
		echo "$i,DRAW"
	fi

	sleep 10
done

rm ai1.log ai2.log cloudwarsx.log
