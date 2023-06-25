#!/bin/bash -v

for suffix in aa ab ac ad
do
  wget --no-check-certificate -c https://zenodo.org/record/8076831/files/web.tar.gz.part${suffix}?download=1 -O web.tar.gz.part${suffix}
done

cat web.tar.gz.part* > web.tar.gz
tar zxvf web.tar.gz
