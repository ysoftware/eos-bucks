#!/bin/bash

for ((i=1;i<100;i++))
do
   	a=$(echo "$i * 0.001 + 2" | bc)
    /usr/local/bin/cleos -v --url http://jungle2.cryptolions.io push action buckbuckbuck open '["yaroslaveroh", "10.0000 EOS", '$a', 0]' -p yaroslaveroh
    sleep 0.1
done