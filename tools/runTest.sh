#! /bin/bash
err(){
	echo "ERROR: $*"
	exit 1
}

usage(){
  err "usage: $0 [-v] [-r] [-s] [-n num=10] [-o open=1] [-w serverPort=8080] chartPattern runs"
}
pdir=`dirname $0`
silent=0
num=10
open=1
serverPort=8080
valgrind=0
render=''
while getopts "rsn:o:w:v" o; do
    case "$o" in
        s)
            silent=1
            ;;
        n)
            num="$OPTARG"
            ;;
        o)
            open="$OPTARG"
            ;;
        w)
            serverPort="$OPTARG"
            ;;
        v)
            valgrind=1
            ;;
        r)
            render='-r'
            ;;
        *)
            usage
            ;;
    esac
done
shift $((OPTIND-1))
pattern="$1"
rc="$2"
if [ "$pattern" != "" -a "$rc" = "" ] ; then
  usage
  exit 1
fi
url="http://localhost:$serverPort"
logs(){
	if [ $silent = 1 ] ; then
		cat > /dev/null
	else
		sed 's/.*/#&/'
	fi

}
EXE="$pdir/../provider/build/debug/AvnavOchartsProvider"
SCRIPT="$pdir/t1.py"
[ ! -x "$EXE" ] && err "$EXE not found"
[ ! -x "$SCRIPT" ] && err "$SCRIPT not found"
echo "running `ls -l $EXE`"
workspaceFolder="$pdir/.."
AVNAV_TEST_KEY=Decrypted
export AVNAV_TEST_KEY
if [ $valgrind = 1 ] ; then
  valgrind --leak-check=yes --num-callers=50 $EXE "-x" "1" "-k" "-d" "2" "-g" "${workspaceFolder}/gui/build/debug" "-t" "${workspaceFolder}/s57data" "${workspaceFolder}/provider/build/test" "$serverPort" -p $$ &
else
  $EXE "-x" "1" "-k" "-d" "2" "-g" "${workspaceFolder}/gui/build/debug" "-t" "${workspaceFolder}/s57data" "${workspaceFolder}/provider/build/test" "$serverPort" -p $$ &
fi
pid=$!
if [ "$pattern" = "" ] ; then
  sleep 5
  echo "$EXE runing with pid $pid at port $serverPort"
  while true
  do
    kill -0 $pid || err "$EXE has stopped"
    ps -o pid,rss,vsz -p $pid
    sleep 5
  done
fi

sleeptime=5
timeout=20
if [ $valgrind = 1 ] ; then
  sleeptime=90
  timeout=60
fi
while [ $sleeptime -gt 0 ]
do
  $SCRIPT -n 0 "$url" "$pattern"
  if [ $? = 0 ] ; then
    break
  fi
  sleep 1
  sleeptime=`expr $sleeptime - 1`
done
kill -0 $pid  || err "testprocess $pid not running"
echo "start test"
ps -o pid,rss,vsz -p $pid
while [ $rc -gt 0 ] 
do
	rc=`expr $rc - 1`
	echo "starting $SCRIPT $render -t $timeout -n $num -o $open $url $pattern"
	$SCRIPT $render -t $timeout -n $num -o $open "$url" "$pattern" | logs
	ps -o pid,rss,vsz -p $pid
done


