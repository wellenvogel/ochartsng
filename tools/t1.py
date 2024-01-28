#! /usr/bin/env python3

from urllib.request import urlopen  
import json
import sys
import getopt
import os

def fetchJson(url: str, timeout:int = 20)-> dict:
	response = urlopen(url,timeout=timeout)
	return json.loads(response.read())

def getChartSet(baseUrl:str,setName:str)->tuple:
	url=baseUrl+"/status"
	data=fetchJson(url)
	charts=data['data']['chartManager']['chartSets']
	FINDKEYS=['name','title','directory']
	for chartSet in charts:
		if not chartSet['active']:
			continue
		for key in FINDKEYS:
			if chartSet['info'][key].find(setName) >= 0:
				return (chartSet['info']['directory'],chartSet['info']['name'])

def getChartNames(dir:str)-> list:
	rt=list(filter(lambda x: True if (x.endswith('oesu') or x.endswith('.oesenc')) else False , os.listdir(dir)))
	rt.sort()
	return rt

def openChart(baseUrl:str, setKey:str,chart:str,render:bool,timeout:int = 20) :
	url=baseUrl+"/charttest/open/"+setKey+"/"+chart
	if render:
		url+="?render=true"
	rs=fetchJson(url,timeout=timeout)
	if rs['status'] != 'OK':
		print("ERROR: unable to open %s: %s"%(chart,rs['info']))
		return False
	else:
		print("open %s ok"%chart)
		if not render:
			return True
		data=rs.get('data')
		if data is None:
			return False
		return data.get('url') is not None

def closeChart(baseUrl:str, setKey: str,chart:str,timeout:int = 20) -> bool:
	rs=fetchJson(baseUrl+"/charttest/close/"+setKey+"/"+chart,timeout=timeout)
	if rs['status'] != 'OK':
		print("ERROR: unable to close %s: %s"%(chart,rs['info']))
		return False
	else:
		print("close %s ok"%chart)
	return True

def usage():
	print("usage: %s [-t timeout=20] [-r] [-o numOpen=1] [-n num=1] baseUrl setName "%sys.argv[0])

if __name__ == '__main__':
	try:
		opts, args = getopt.getopt(sys.argv[1:], "t:ro:n:")
	except getopt.GetoptError as err:
		# print help information and exit:
		print(err)  # will print something like "option -a not recognized"
		usage()
		sys.exit(2)
	if len(args) < 2:
		usage()
		sys.exit(2)
	baseUrl=args[0]
	setName=args[1]
	print("getChartSet for %s"%setName)
	setInfo=getChartSet(baseUrl,setName)
	if setInfo is None:
		raise Exception("unable to find set %s"%setName)
	print("using chartSet %s"%setInfo[0])
	num=1
	maxOpen=1
	render=False
	timeout=20
	for o,v in opts:
		if o == '-n':
			num=int(v)
		if o == '-o':
			maxOpen=int(v)
		if o == '-t':
			timeout=int(v)
		if o == '-r':
			render=True
	if maxOpen > num:
		maxOpen=num
	tile=None
	charts=getChartNames(setInfo[0])
	if len(charts) < num:
		raise Exception("not enough charts in %s, expected %d found %d"%(setInfo[0],num,len(charts)))
	print("num=%d, open=%d"%(num,maxOpen))
	openCharts=[]
	setKey=setInfo[1]
	idx=0
	while num > 0:
		chart=charts[idx]
		idx+=1
		num-=1
		if not openChart(baseUrl,setKey,chart,render,timeout=timeout):
			continue
		openCharts.append(chart)
		if len(openCharts) >= maxOpen:
			clchart=openCharts.pop(0)
			closeChart(baseUrl,setKey,clchart,timeout=timeout)
	for oc in openCharts:
		closeChart(baseUrl,setKey,oc,timeout=timeout)

'''
while [ $num -gt 0 ]
do
	for CHART in $CHARTLIST
	do
		if [ $num -lt 1 ] ; then
			break
		fi
		echo "$CHART"
		curl -s "http://localhost:8080/charttest/open/$CS/$CHART"
		curl -s "http://localhost:8080/charttest/close/$CS/$CHART"
		num=`expr $num - 1`
	done
done
'''
