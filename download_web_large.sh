#!/bin/bash -v

for suffix in aa ab ac ad ae af ag ah ai aj ak al am an ao ap aq ar as at au av aw ax ay az
do
  wget --no-check-certificate -c https://zenodo.org/record/8088562/files/web_large.tar.gz.part${suffix}?download=1 -O web_large.tar.gz.part${suffix}
done

cat web_large.tar.gz.part* > web_large.tar.gz
tar zxvf web_large.tar.gz
