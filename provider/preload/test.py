#! /usr/bin/env python3
import struct
import socket
import sys
import os
import time
PUBLIC_FIFO="/tmp/OCPN_PIPEX"

def sendCommandFifo(cmd,fileName,key,outFile):
  cmdNum=int(cmd)
  print("sending command %d"%cmdNum)
  bytes=struct.pack("b256s256s512s",cmdNum,outFile.encode('utf-8'),fileName.encode('utf-8'),key.encode('utf-8'))
  with open(PUBLIC_FIFO,"wb") as fifo:
    fifo.write(bytes)
    fifo.close()
  sz=0  
  while True:
    time.sleep(2)  
    szn=os.stat(outFile).st_size
    if szn != sz:
        sz=szn
    else:
        break
  print("file %s with %d bytes"%(outFile,sz))      



def sendCommand(cmd,fileName,key,outFile,address,family=socket.AF_INET):
    cmdNum=int(cmd)
    print("test: sending command %d"%cmdNum)
    ch=socket.socket(family, socket.SOCK_STREAM)
    ch.connect(address)
    bytes=struct.pack("b256s256s512s",cmdNum,outFile.encode('utf-8'),fileName.encode('utf-8'),key.encode('utf-8'))
    ch.send(bytes)
    numbytes=0
    while True:
        data=ch.recv(100)
        if data == b'':
            print("test: channel closed after %d bytes"%numbytes)
            break
        numbytes+=len(data)
        #print(data)
def touch(path):

    basedir = os.path.dirname(path)
    if basedir:
        if not os.path.exists(basedir):
            os.makedirs(basedir)
    with open(path, 'a'):
        os.utime(path, None)

if __name__ == '__main__':
    if len(sys.argv) != 5:
        print("usage: %s cmd oesencFile key tcp:host:port"%sys.argv[0])
        print("       %s cmd oesencFile key local:name"%sys.argv[0])
        print("       %s cmd oesencFile key file:outFile"%sys.argv[0])
        sys.exit(1)
    mode=sys.argv[4]
    if mode.startswith("file:"):
        fname=mode[5:]
        if os.path.exists(fname):
            st=os.stat(fname)
            if st.st_size > 0:
                print(fname,"already exists")
                sys.exit(1)
        else:
            touch(fname)
        sendCommandFifo(sys.argv[1],sys.argv[2],sys.argv[3],fname)            
        sys.exit(0)
    if mode.startswith("tcp:"):
        (host,port)=mode[4:].split(":")        
        sendCommand(sys.argv[1],sys.argv[2],sys.argv[3],"dummy",(host,int(port)))
        sys.exit(0)
    if mode.startswith("local:"):
        addr=b'\x00'+mode[6:].encode()
        sendCommand(sys.argv[1],sys.argv[2],sys.argv[3],"dummy",addr,family=socket.AF_UNIX)
        sys.exit(0)
    print("invalid mode",mode)
    sys.exit(1)    


