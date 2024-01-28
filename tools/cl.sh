#! /bin/bash
x=1
while [ $x -le 255 ]
do
	echo "$((255*256/x)),"
	x=$((x+1))
done
