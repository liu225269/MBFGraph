#!/bin/bash -v
for size in 1 1M 10M
do
  wget --no-check-certificate -c https://zenodo.org/record/8076831/files/add_${size}.tar.gz?download=1 -O add_${size}.tar.gz
  tar zxvf add_${size}.tar.gz
done
