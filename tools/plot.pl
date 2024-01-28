#!/usr/bin/env perl
use strict;
my $first=1;
my $lx=0;
my $ly=0;
my $mode=2;
my $point="";
my $start;
my $end;
while (scalar @ARGV){
  if ($ARGV[0] eq "-4"){
    $mode=4;
    shift;
    next;
  }
  if ($ARGV[0] eq "-n"){
    $start="##Start$ARGV[1]";
    $end="##End$ARGV[1]";
    shift;
    shift;
    next;
  }
  last;
}
if (scalar @ARGV >= 2){
  $point="$ARGV[0],$ARGV[1]";
  print("point=$point\n");
  shift;
  shift;
}
my @plotcmd=("gnuplot","-p");
if ($point){
  push(@plotcmd,"-e","set label 1 \"ref\" at $point point");
}
push(@plotcmd,"-e","plot '-' using 1:2:3:4 with vectors");
print("##mode=$mode,start=$start,end=$end plotting: ".join(" ",@plotcmd)."##\n");
#my @plotcmd=("cat");
open(my $plot,"|-",@plotcmd) or die "cannot open ".join(" ",@plotcmd);
my $lastx=0;
my $lasty=0;
my $active=1;
if ($start ne ""){
  $active=0;
}
while (<>){
  chomp;
  if ( $active == 0){
    if ($_ =~ /.*$start.*/){
      print("##Start\n");
      $active=1;
      next;
    }
  }
  else{
    if ($end ne "" ){
      if ($_ =~ /$end/){
        $active=0;
        next;
      }
    }
  }
  if ($active == 0){
    next;
  }
  s/[^0-9 -]*//g;
  s/^ *//;
  print("##$_##\n");
  if ($mode == 4){
    print $plot $_."\n";
          my @l=split(/  */);
          if (scalar(@l) >= 4){
      $lastx=$l[0]+$l[2];
      $lasty=$l[1]+$l[3];
    }
    next;
  }
  my @l=split(/  */);
  if (scalar(@l) < 2){
    next;
  }
  if ($first){
    $first=0;
    $lx=$l[0];
    $ly=$l[1];
  }
  else{
    my $dx=$l[0]-$lx;
    my $dy=$l[1]-$ly;
    print $plot "$lx $ly $dx $dy\n";
    print "plot $lx $ly $dx $dy\n";
    $lx=$l[0];
    $ly=$l[1];
  }
}
if ($mode == 4){
  print("##last: $lastx $lasty\n");
}
print("##done\n");
