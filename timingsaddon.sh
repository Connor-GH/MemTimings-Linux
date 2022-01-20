#!/bin/bash

printf "MemTimings For Linux\n"
#I am not affiliated with irusanov or ZenTimings in any way.
#I just can't come up with a creative name.
#Here is his program (Windows) https://github.com/irusanov/ZenTimings
#This file was created by me, and the C file associated with it was created from Cyring on Github from this repo:
					#https://github.com/irusanov/ZenTimings (under master/C/zencli.c)
printf "Timings:\n\n"

B='\033[1;34m'
W='\033[0m'

MCLK=$(sudo ./timings smu 0x50201 | sed -sn 3p | sed 's/ //g' | cut -c 58-65)
tRDRDSCL=$(sudo ./timings smu 0x50221 | sed -sn 3p | sed 's/ //g' | cut -c 37-40)

	printf "MCLK:\t\t$B$(expr $((2#$MCLK)) \* 200 / 3)$W\t\ttRDRDSCL:\t$B$((2#$tRDRDSCL))$W\n"

CMD_RATE=$(sudo ./timings smu 0x50201 | sed -sn 3p | sed 's/ //g' | cut -c 54-56)
tWRWRSCL=$(sudo ./timings smu 0x50225 | sed -sn 3p | sed 's/ //g' | cut -c 35-40)
	printf "CMD:\t\t$B$((2#$CMD_RATE))T$W\t\ttWRWRSCL:\t$B$((2#$tWRWRSCL))$W\n"

GDM=$(sudo ./timings smu 0x50201 | sed -sn 3p | sed 's/ //g' | cut -c 53-54)
tCWL=$(sudo ./timings smu 0x50215 | sed -sn 3p | sed 's/ //g' | cut -c 59-65)


	if [ "$((2#$GDM))" -eq "2" ]; then
	printf "GDM:\t\t${B}Enabled${W}\t\t"
	else
	printf "GDM:\t\t${B}Disabled${W}\t\t"
	fi
	printf "tCWL:\t\t$B$((2#$tCWL))$W\n\n"

tCL=$(sudo ./timings smu 0x50205 | sed -sn 3p | sed 's/ //g' |  cut -c 59-65)
tRTP=$(sudo ./timings smu 0x5020d | sed -sn 3p | sed 's/ //g' |  cut -c 36-40)
	printf "tCL:\t\t$B$((2#$tCL))$W\t\ttRTP:\t\t$B$((2#$tRTP))$W\n"

tRCDWR=$(sudo ./timings smu 0x50205 | sed -sn 3p | sed 's/ //g' | cut -c 35-40)
tRDWR=$(sudo ./timings smu 0x50229 | sed -sn 3p | sed 's/ //g' | cut -c 52-56)
	printf "tRCDWR:\t\t$B$((2#$tRCDWR))$W\t\ttRDWR:\t\t$B$((2#$tRDWR))$W\n"

tRCDRD=$(sudo ./timings smu 0x50205 | sed -sn 3p | sed 's/ //g' | cut -c 42-48)
tWRRD=$(sudo ./timings smu 0x50229 | sed -sn 3p | sed 's/ //g' | cut -c 61-65)
	printf "tRCDRD:\t\t$B$((2#$tRCDRD))$W\t\ttWRRD:\t\t$B$((2#$tWRRD))$W\n"

tRP=$(sudo ./timings smu 0x50209 | sed -sn 3p | sed 's/ //g' | cut -c 42-48)
tRDRDSC=$(sudo ./timings smu 0x50221 | sed -sn 3p | sed 's/ //g' | cut -c 45-48)
	printf "tRP:\t\t$B$((2#$tRP))$W\t\ttRDRDSC:\t$B$((2#$tRDRDSC))$W\n"

tRAS=$(sudo ./timings smu 0x50205 | sed -sn 3p | sed 's/ //g' | cut -c 50-56)
tRDRDSD=$(sudo ./timings smu 0x50221 | sed -sn 3p | sed 's/ //g' | cut -c 53-56)
	printf "tRAS:\t\t$B$((2#$tRAS))$W\t\ttRDRDSD:\t$B$((2#$tRDRDSD))$W\n"

tRC=$(sudo ./timings smu 0x50209 | sed -sn 3p | sed 's/ //g' | cut -c 57-65)
tRDRDDD=$(sudo ./timings smu 0x50221 | sed -sn 3p | sed 's/ //g' | cut -c 61-65)
	printf "tRC:\t\t$B$((2#$tRC))$W\t\ttRDRDDD:\t$B$((2#$tRDRDDD))$W\n\n"

tRRDS=$(sudo ./timings smu 0x5020d | sed -sn 3p | sed 's/ //g' | cut -c 60-65)
tWRWRSC=$(sudo ./timings smu 0x50225 | sed -sn 3p | sed 's/ //g' | cut -c 45-48)
	printf "tRRDS:\t\t$B$((2#$tRRDS))$W\t\ttWRWRSC:\t$B$((2#$tWRWRSC))$W\n"

tRRDL=$(sudo ./timings smu 0x5020d | sed -sn 3p | sed 's/ //g' | cut -c 52-56)
tWRWRSD=$(sudo ./timings smu 0x50225 | sed -sn 3p | sed 's/ //g' | cut -c 53-56)
	printf "tRRDL:\t\t$B$((2#$tRRDL))$W\t\ttWRWRSD:\t$B$((2#$tWRWRSD))$W\n"

tFAW=$(sudo ./timings smu 0x50211 | sed -sn 3p | sed 's/ //g' | cut -c 58-65)
tWRWRDD=$(sudo ./timings smu 0x50225 | sed -sn 3p | sed 's/ //g' | cut -c 61-65)
	printf "tFAW:\t\t$B$((2#$tFAW))$W\t\ttRDRDDD:\t$B$((2#$tWRWRDD))$W\n"

tWTRS=$(sudo ./timings smu 0x50215 | sed -sn 3p | sed 's/ //g' | cut -c 52-56)
tCKE=$(sudo ./timings smu 0x50254 | sed -sn 3p | sed 's/ //g' | cut -c 36-40)
	printf  "tWTRS:\t\t$B$((2#$tWTRS))$W\t\ttCKE:\t\t$B$((2#$tCKE))$W\n"

tWTRL=$(sudo ./timings smu 0x50215 | sed -sn 3p | sed 's/ //g' | cut -c 42-48)
tREFI=$(sudo ./timings smu 0x50231 | sed -sn 3p | sed 's/ //g' | cut -c 49-65)
MTs="`echo "$(expr $((2#$MCLK)) \* 200 / 3)"`"
	printf "tWTRL:\t\t$B$((2#$tWTRL))$W\t\ttFREFI:\t\t$B$((2#$tREFI))$W\n"

tWR=$(sudo ./timings smu 0x50219 | sed -sn 3p | sed 's/ //g' | cut -c 58-65)
tREFItCK=$(echo "$((2#$tREFI))")
	printf "tWR:\t\t$B$((2#$tWR))$W\t\ttREFI (ns):\t$B$(echo "$tREFItCK * 2000 / $MTs" | bc -l | cut -c 1-7)$W\n\n"

tRFC1=$(sudo ./timings smu 0x50261 | sed -sn 3p | sed 's/ //g' | cut -c 54-65)
tRFCtCK=`echo "$((2#$tRFC1))"`
	printf "tRFC (ns):\t$B$(echo "$tRFCtCK * 2000 / $MTs" | bc -l | cut -c 1-3)$W\n"
	printf "tRFC1:\t\t$B$((2#$tRFC1))$W\n"

tRFC2=$(sudo ./timings smu 0x50261 | sed -sn 3p | sed 's/ //g' | cut -c 43-53)
	printf "tRFC2:\t\t$B$((2#$tRFC2))$W\n"

tRFC4=$(sudo ./timings smu 0x50261 | sed -sn 3p | sed 's/ //g' | cut -c 33-42)
	printf "tRFC4:\t\t$B$((2#$tRFC4))$W\n"

tMOD=$(sudo ./timings smu 0x50234 | sed -sn 3p | sed 's/ //g' | cut -c 51-56)
	printf "tMOD:\t\t$B$((2#$tMOD))$W\n"
