#!/bin/bash

#Check if running as root
if [[ $(id -u) -ne 0 ]]; then
  echo "Please run as root"
  exit
fi

mkdir -p /var/www/html/data
while :;
do
    ./drsLog $(cat config.txt);
    cat /var/www/html/data/* > /var/www/html/all.dat
done;
