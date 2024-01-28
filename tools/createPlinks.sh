#!/bin/bash
#create symlinks for using the dev env as an avnav plugin
#call this script from within a directory you created below avnav_data_dir/plugins
PDIR=`dirname $0`
BASE=`realpath --relative-to=. $PDIR/..`
guibase=debug
[ "$1" = "release" ] && guibase=release

log(){
  echo "INFO: $*"
}
clink(){
  if [ -e "$1" ] ; then
    log "remove existing $1"
    rm -f "$1"
  fi
  log "linking $1 to $BASE/$2"
  ln -s "$BASE/$2" "$1"
}

clink gui gui/build/$guibase
bb="provider/build/debug"
for f in AvnavOchartsProvider libpreload.so oexserverd
do
  clink $f $bb/$f
done
for f in plugin.py provider.sh
do
  clink $f avnav-plugin/$f
done
clink s57data s57data

