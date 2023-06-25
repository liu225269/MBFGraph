#!/bin/bash -v

DATA=web_large
DATA2=web_large2
MAX=2700000
#MAX=1
KERNEL=CPU
PROCE=8


  for index in 15 #{16..1}
  do
    for opt_bit in 256 #8 16 32 64 128 512 1024 2048
    do
      for PAGE in 2048 #512 1024 4096 8192 #16384
      do
        BFs=$((PAGE / 8))
        BFs2=$((PAGE / 16))
        for BIT in 512 128 1024 2048 256
        do
          SUFFIX=${DATA}_bf${BIT}_${IN_DIR}_o${opt_bit}_${index}_pg${BFs}
	  
          COMMON1=" --measure_scatter_gather --heartbeat -a -p ${PROCE} --page_size ${PAGE} --bits_per_BF ${BIT} "
          COMMON2=" --chip_v_once 4 --max_check_v ${MAX} --kernel ${KERNEL}"
          COMMON=${COMMON1}${COMMON2}
  
          #rm sssp_${SUFFIX}
          #./bin/benchmark_driver -g ${DATA2} -b sssp --sssp::source 70 --physical_memory 4294967296 \
	  #  --bfs_per_page ${BFs2} --bf_mode create ${COMMON} >> sssp_${SUFFIX} 2>&1
  
	  rm bfs_${SUFFIX}
          ./bin/benchmark_driver -g ${DATA} -b bfs --bfs::root 70 --physical_memory 4294967296 \
	    --bfs_per_page ${BFs} --bf_mode create ${COMMON} >> bfs_${SUFFIX} 2>&1
	  
	  #rm page_${SUFFIX}
          #./bin/benchmark_driver -g ${DATA} -b pagerank_ddf --pagerank::niters 30 --physical_memory 4294967296 \
	  #  --bfs_per_page ${BFs} --bf_mode create ${COMMON} >> page_${SUFFIX} 2>&1
  
          #rm cc_${SUFFIX}
          #./bin/benchmark_driver -g ${DATA} -b cc --physical_memory 4294967296 \
	  #  --bfs_per_page ${BFs} --bf_mode create ${COMMON} >> cc_${SUFFIX} 2>&1
          
        done
      done
    done
  done
