#! /bin/bash
err(){
  echo "ERROR: $*"
  exit 1
}

[ "$1" = "" ] && err "usage: $0 pid"
cat /proc/$1/smaps| perl -e '$s=0;while(<>){if ($_ =~ /^Rss/){@p=split(/  */,$_);$s=$s+$p[1];}}print $s."\n";'

