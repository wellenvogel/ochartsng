#! /bin/bash
num="$1"
[ "$num" = "" ] && num=10
CS="CSI_f513307cf36e0b19fbb46d2cea43e92d_oeuSENC-DE-2023-1-2-base"
read -r -d '' CHARTLIST <<'CL'
OC-49-SLGON5.oesu
OC-49-SLHSO5.oesu
OC-49-SLKSO5.oesu
OC-49-SLSON5.oesu
OC-49-SNBON5.oesu
OC-49-SORSO5.oesu
OC-49-SRFON5.oesu
OC-49-STKSO6.oesu
OC-49-STSON5.oesu
OC-49-STSSO6.oesu
OC-49-SUGSO6.oesu
OC-49-SUHON5.oesu
OC-49-SUWSO6.oesu
OC-49-T41ON4.oesu
OC-49-T51ON4.oesu
OC-49-TDWSO5.oesu
OC-49-TIVSO5.oesu
OC-49-TROSO5.oesu
OC-49-TSNSO5.oesu
OC-49-UBWSO5.oesu
OC-49-VHBON5.oesu
OC-49-VHWON5.oesu
OC-49-WGBSO5.oesu
OC-49-WHBSO5.oesu
OC-49-WLPON5.oesu
OC-49-WLSSO5.oesu
OC-49-WRPSO6.oesu
OC-49-XTSSO5.oesu
OC-49-XUCON5.oesu
OC-49-YDNON5.oesu
CL
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

