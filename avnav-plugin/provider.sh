#! /bin/sh
PDIR=`dirname $0`
PDIR=`readlink -f $PDIR`
ENVFILE=$PDIR/environment.txt
workdir=$PDIR
#uncomment the next line to get core files
ulimit -c unlimited
if [ -f $ENVFILE ] ; then
    echo "$ENVFILE found, souring it"
    . $ENVFILE
fi
if [ "$AVNAV_PROVIDER_WORKDIR" != "" ] ; then
    if [ -d $AVNAV_PROVIDER_WORKDIR ] ; then
        workdir="$AVNAV_PROVIDER_WORKDIR"
    fi
fi
echo "working in $workdir"
cd $workdir
if [ "$AVNAV_DEBUG_OCHARTS" != "" ] ; then
    kflag="-k"
else
    kflag=""
fi
exec $PDIR/AvnavOchartsProvider $kflag $*
   
