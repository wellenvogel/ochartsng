#! /bin/sh
uid=`id -u`
if [ "$uid" != "0" ] ; then
  echo only root can run this
  exit 1
fi
echo installing...
apt-get install -y binfmt-support qemu-user-static
#we need to ensure the "F" flag on the binfmt entry for our cross architecture so that docker commands can use this
echo checking binfmt set up for docker
cat /proc/sys/fs/binfmt_misc/qemu-arm | grep flags | grep F
if [ $? != 0 ] ; then
  echo fixing binfmt set up fro arm
  echo -1 > /proc/sys/fs/binfmt_misc/qemu-arm
  echo ':qemu-arm:M:0:\x7f\x45\x4c\x46\x01\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x02\x00\x28\x00:\xff\xff\xff\xff\xff\xff\xff\x00\xff\xff\xff\xff\xff\xff\xff\xff\xfe\xff\xff\xff:/usr/bin/qemu-arm-static:OCF' > /proc/sys/fs/binfmt_misc/register
  echo rechecking
  cat /proc/sys/fs/binfmt_misc/qemu-arm | grep flags | grep F
  if [ $? != 0 ] ; then
    echo "unable to correctly set up binfmt"
    exit 1
  fi
fi

cat /proc/sys/fs/binfmt_misc/qemu-aarch64 | grep flags | grep F
if [ $? != 0 ] ; then
  echo fixing binfmt set up for aarch64
  echo -1 > /proc/sys/fs/binfmt_misc/qemu-aarch64
  echo ':qemu-aarch64:M:0:\x7f\x45\x4c\x46\x02\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x02\x00\xb7\x00:\xff\xff\xff\xff\xff\xff\xff\x00\xff\xff\xff\xff\xff\xff\xff\xff\xfe\xff\xff\xff:/usr/bin/qemu-aarch64-static:OCF' > /proc/sys/fs/binfmt_misc/register
  echo rechecking
  cat /proc/sys/fs/binfmt_misc/qemu-aarch64 | grep flags | grep F
  if [ $? != 0 ] ; then
    echo "unable to correctly set up binfmt"
    exit 1
  fi
fi
echo preparation done
  

