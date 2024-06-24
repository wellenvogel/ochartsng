#! /bin/bash
res=""
base="61766f68617274736e67020608020401"
err(){
	echo "ERROR: $*"
	exit 1
}
if [ "$ANDROID_HOME" != "" ] ; then
	cmd="$ANDROID_HOME/platform-tools/adb"
else
	cmd=adb
fi
echo "trying adb command $cmd"
res="`$cmd shell ps -ef | grep oexserver`"
if [ "$res" = "" ] ; then
	echo "unable to find a running license daemon (oexserverd)"
	exit 1
fi
res="`echo $res | sed 's/.*oexserverd.so *//' | sed 's/ *;.*//'| sed 's/ *-u//' |sed 's/  */:/g'`"
echo RESULT:
echo "$res"
if [ "$1" != "" ] ; then
	echo "writing $1"
	echo -n "$res"  | openssl aes-128-ecb -K "$base" -out $1 || err "unable to write $1"
fi

