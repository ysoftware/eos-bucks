#!/bin/bash

for ((i=1;i<100;i++))
do
   	a=$(echo "200 + $i" | bc)
    /usr/local/bin/cleos -v --url http://jungle2.cryptolions.io push action scrugeosbuck open '["yaroslaveroh", "100.0000 EOS", '$a', 0]' -p yaroslaveroh
    sleep 0.1
done
