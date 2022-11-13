#!/bin/bash

echo "Вывести список всех, кто присутствовал на контрольной работе по некоторому  предмету по определённой теме (например по предмету Unix тема - grep):"
awk "{print $1}" test/Unix_Grep_10.10.2006

echo "Вывести среднюю оценку по каждой из контрольных:"
for file in test/*; do
  awk 'BEGIN{count=0;sum=0}{count+=1;sum+=$2}END{split(FILENAME,dir,"/");split(dir[2],sp,"_");printf("Среднее по предмету %s на контрольной по %s: %f\n", sp[1], sp[2], sum/count)}' $file
done

echo "Вывести список всех, кто справился с работой на зачёт:"
awk '{if ($2 > 2) {print $1}}' test/Unix_Grep_10.10.2006

echo "Вывести все темы по какому-либо предмету в алфавитном порядке:"
ls test/Unix* | awk 'BEGIN{FS="_"}{print $2 | "sort"}'