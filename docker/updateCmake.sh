#! /bin/sh
# see https://www.claudiokuenzler.com/blog/796/install-upgrade-cmake-3.12.1-ubuntu-14.04-trusty-alternatives
FULLVERSION=3.12.1
VERSION=v3.12
me=`id -u`
err(){
    echo "ERROR: $*"
    exit 1
}
if [ "$me" != 0 ]; then
    err must be run as root
fi
apt-get update
apt-get install wget
cd 
url="http://www.cmake.org/files/$VERSION/cmake-$FULLVERSION.tar.gz"
fname="cmake-$FULLVERSION.tar.gz"
dir="cmake-$FULLVERSION"
[ -f "$fname" ] && rm -f $fname
[ -d "$dir" ] && rm -rf "$dir"
wget "$url" || err "unable to download $url"
[ ! -f "$fname" ] && err "$fname not found after download"
tar -xvzf "$fname" || err "unable to untar"
[ ! -d "$dir" ] && err "$dir not found after unpack"
cd $dir || err "unable to cd into $dir"
./configure || err configure failed
make install || err make failed
update-alternatives --install /usr/bin/cmake cmake /usr/local/bin/cmake 1 --force || err "alternatives failed"
echo "cmake installed"
cmake --version
