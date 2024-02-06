#! /bin/bash
#script to collect the s57 file that OpenCPN has generated after it started 
#with -parse_all_enc
#set -x
usage(){
    [ "$1" != "" ] && echo "ERROR: $*"
    echo "usage: $0 [-n chartname] [-o opencpndir]   [-f] s57Dir outzip"
    exit 1
}
err(){
    echo "ERROR: $*"
    exit 1
}

opencpn="$HOME/.opencpn"
chartname=""
force=0

while getopts "n:o:f" o; do
    case "${o}" in
        n)
            chartname=${OPTARG}
            ;;
        o)
            opencpn=${OPTARG}
            ;;
        f)
            force=1
            ;;
        *)
            usage
            ;;
    esac
done
shift $((OPTIND-1))

[ $# -ne 2 ] && usage
[ ! -d "$1" ] && usage "directory $1 not found"
if [ -e "$2" ] ; then
  [ $force != 1 ] && usage "outfile $2 already exists, use -f to overwrite"
fi
sencdir="$opencpn/SENC"
[ ! -d "$sencdir" ] && err "$sencdir not found - let OpenCPN create the .s57 files before"
fullName=$(basename -- "$2")
extension="${fullName##*.}"
filename="${fullName%.*}"
if [ "$cname" = "" ] ; then
    echo "using chartname $filename"
    cname=$filename
fi


tmp=/tmp/cs57_$$

trapfkt(){
    if [ -d "$tmp" ] ; then
        echo "cleaning up $tmp"
        rm -rf $tmp
    fi
}
trap trapfkt 0
zipdir="$tmp/$cname"
mkdir -p "$zipdir" || err "unable to create $zipdir"
find  $1 -type f | while read f
do
    if [[ "$f" =~ \.[0-9][0-9][0-9]$ ]] ; then
        echo "trying $f"
        bn="${f##*/}"
        fb="${bn%.*}"
        echo "base=$fb"
        hasCopied=0
        for fs57 in `find "$sencdir" -maxdepth 1 -name "*$fb.S57"`
        do
            echo "using $fs57"
            cp "$fs57" "$zipdir" || err "error copying $fs57 $zipdir"
            hasCopied=1
        done
        [ $hasCopied != 1 ] && echo "WARNING: no S57 file for $fb"
    fi
done
ls "$zipdir"/* > /dev/null 2>&1 
if [ $? != 0 ] ; then
    echo "no files collected, doe not create $2"
else
    echo "ChartInfo:$cname" > "$zipdir/Chartinfo.txt"
    if [ -f "$2" ]; then
        rm -f "$2" || err "unable to remove $2"
    fi
    outname=`realpath $2`
    (cd $tmp && zip -o "$outname" -r "$cname" ) || err "error creating outfile $outname"
    echo "$outname created"
fi

