#!/bin/bash

echo "Вывести на экран все строки, которые относятся к только пройденным тестам."
sed -n '/^Accepted/p' result.txt
echo

echo "После каждого непройденного теста вставить строчку Test Again."
sed -e '/^Not Accepted/ s/$/ Test Again/' result.txt
echo

echo "Вывести все строчки, соответствующие непройденным тестам."
sed -e '/^Accepted/d' result.txt
echo

echo "Вместо строк с непройденными тестами вставить строчку Warning."
sed -e "s/^Not Accepted.*/Warning/" result.txt
echo