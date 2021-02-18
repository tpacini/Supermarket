#!/bin/bash

pid=$(pidof supermercato)
lsof -p $pid +r 1 &>/dev/null

# Ottengo il nome del file di log
string=$(tail -1 lib/config.txt)
IFS=' : '
tokens=( $string )
filename=${tokens[3]}

# Ricavo le linee scritte sul file di log
lines=$(wc -l  $filename)
IFS=' '
tokens=( $lines )

nline=${tokens[0]}
last=$(head -1 $filename)
first=$((nline-last+1))

IFS="\n"
n=1
echo -e "| id cl.| #prod | t. super. |  t. coda  | #code |"
while read line; do
if [ "$n" -eq "$first" ]; then 
    echo -e "\n\n| id ca.| #prod |#clien.| t. apert. |t.serv.|#chius.|"
fi
if [ "$n" -eq 1 ]; then 
    n=$((n+1)) 
else 
    echo $line  
    n=$((n+1)) 
fi
done < $filename
