#!/bin/bash

pid=$(pidof director)
lsof -p $pid +r 1 &>/dev/null

filename='log.txt'

# Compute log's lines
lines=$(wc -l $filename)
IFS=' '
tokens=( $lines )

nline=${tokens[0]}
#last=$(head -1 $filename)
#first=$((nline-last+1))
#
#IFS="\n"
#n=1
#echo -e "| id cl.| #prod | t. super. |  t. coda  | #code |"
#while read line; do
#if [ "$n" -eq "$first" ]; then 
#    echo -e "\n\n| id ca.| #prod |#clien.| t. apert. |t.serv.|#chius.|"
#fi
#if [ "$n" -eq 1 ]; then 
#    n=$((n+1)) 
#else 
#    echo $line  
#    n=$((n+1)) 
#fi
#done < $filename
