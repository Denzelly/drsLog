#!/bin/bash
mkdir -p /var/www/html/data
while :;
do
    ./drsLog $(cat config.txt);
    cat /var/www/html/data/* > /var/www/html/data/all.dat
done;
