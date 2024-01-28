#! /bin/bash
#set -x

usage(){
	echo "usage: $0 [-c] [-d] [-a] [-i] [-v avnaVersion]  builddir dockerImageName"
}

#relative path of cmakelist dir from this script
SRCDIR=.
doClean=0


ttyopt="-t"
bFlag=""
CMAKE_MODE=Release
internal=0
avnavVersion=unset
while getopts caidv: opt
do
	case $opt in
		c)
		doClean=1
		;;
		a)
		ttyopt=""
		;;
		d)
		CMAKE_MODE=Debug
		;;
		i)
		internal=1
		;;
		v)
		avnavVersion=$OPTARG
		;;
		*)
		usage
		exit 1
		;;
	esac
done

shift `expr $OPTIND - 1`

if [ "$1" = "" ] ; then
  usage
  exit 1
fi

imagename="$2"
container=`echo "$imagename" | tr -d '/:'`
builddir="$1"
PDIR=`dirname $0`
PDIR=`readlink -f "$PDIR"`


if [ $internal = 0 ] ; then
    if [ "$imagename" = "" ] ; then
        usage
        exit 1
    fi
	CONTAINER_RUNNING=$(docker ps --filter name="${container}" -q)
	CONTAINER_EXISTS=$(docker ps -a --filter name="${container}" -q)
	IMAGE_EXISTS=$(docker images -q "$imagename")
	if [ "$CONTAINER_RUNNING" != "" ] ; then
		echo "the container $container is already running"
		exit 1
	fi
	if [ "$CONTAINER_EXISTS" != "" ] ; then
		echo "the container $container already exists"
		exit 1
	fi
	if [ "$IMAGE_EXISTS" = "" ] ; then
		echo "the image $imagename does not exist"
		exit 1
	fi
	cflag=""
	[ $doClean = 1 ] && cflag="-c"
	set -x
	user=`id -u`
	group=`id -g`
	docker run --rm --name "$container" -i $ttyopt  -v $PDIR:/src -u $user:$group "$imagename" /bin/bash -c "cd /src && ./build.sh $bFlag $cflag -v $avnavVersion -i $builddir"
	exit $?
fi
BUILD_DIR="$PDIR/build/$builddir"
if [ $doClean  = 1 ] ; then
	if [ -d "$BUILD_DIR" ] ; then
		echo "cleaning $BUILD_DIR"
		rm -rf "$BUILD_DIR"
	fi
fi
if [ ! -d "$BUILD_DIR" ] ; then
	mkdir -p $BUILD_DIR || exit 1
	doClean=1
fi
cd $BUILD_DIR || exit 1
if [ ! -f Makefile ] ; then
    doClean=1
fi
cmake -DCMAKE_BUILD_TYPE=$CMAKE_MODE -DAVNAV_VERSION=$avnavVersion -DNO_TEST=on ../..
make



