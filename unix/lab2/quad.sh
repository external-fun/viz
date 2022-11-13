#!/bin/bash

A=$1
B=$2
C=$3

D=$(echo "scale=4;$B*$B - 4*$A*$C" | bc)
if [ $D -eq 0 ]; then
  echo "Один корень:"
  echo "-$B / 2*$A" | bc -l
elif [ $D -gt 0 ]; then
  echo "Два корня:"
  echo "scale=4;(-($B) + sqrt($D))/2*$A" | bc -l
  echo "scale=4;(-($B) - sqrt($D))/2*$A" | bc -l
else
  echo "Мнимые корни:"
  M=$(echo "scale=4;sqrt(-($D)) / 2*$A" | bc -l)
  R=$(echo "scale=4;-($B) / 2*$A" | bc -l)
  echo "$R+${M}i"
  echo "$R+${M}i"
fi