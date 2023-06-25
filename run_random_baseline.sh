#!/bin/bash -v

DATA=hello
DATA2=hello2
MAX=600000
#MAX=1
KERNEL=CPU
PROCE=8


  for index in 15 #{16..1}
  do
    for opt_bit in 256 #8 16 32 64 128 512 1024 2048
    do
      for PAGE in 2048 #512 1024 2048 4096 8192 #16384
      do
        BFs=$((PAGE / 8))
        BFs2=$((PAGE / 16))
        for BIT in 256 #128 512 1024 #2048
        do
          SUFFIX=${DATA}_bf${BIT}_${IN_DIR}_o${opt_bit}_${index}_pg${BFs}
	  
          COMMON1=" --measure_scatter_gather --heartbeat -a -p ${PROCE} --page_size ${PAGE} --bits_per_BF ${BIT} "
          COMMON2=" --chip_v_once 4 --max_check_v ${MAX} --kernel ${KERNEL}"
          COMMON=${COMMON1}${COMMON2}

          rm degree_${SUFFIX}_*
	  for MEM in 1073741824 536870912 268435456
	  do
            ./bin/benchmark_driver -g ${DATA} -b degree_cnt --physical_memory ${MEM} \
	      --bfs_per_page ${BFs} --bf_mode create ${COMMON} >> degree_${SUFFIX}_${MEM} 2>&1
          done 

          #rm sssp_${SUFFIX}
          #./bin/benchmark_driver -g ${DATA2} -b sssp --sssp::source 33019544 --physical_memory 1073741824 \
	  #  --bfs_per_page ${BFs2} --bf_mode use ${COMMON} >> sssp_${SUFFIX} 2>&1
  
          #rm cc_${SUFFIX}
          #./bin/benchmark_driver -g ${DATA} -b cc --physical_memory 1073741824 \
	  #  --bfs_per_page ${BFs} --bf_mode use ${COMMON} >> cc_${SUFFIX} 2>&1
  
          #rm page_${SUFFIX}
          #./bin/benchmark_driver -g ${DATA} -b pagerank_ddf --pagerank::niters 30 --physical_memory 1073741824 \
	  #  --bfs_per_page ${BFs} --bf_mode use ${COMMON} >> page_${SUFFIX} 2>&1

          rm bfs_${SUFFIX}
          ./bin/benchmark_driver -g ${DATA} -b bfs --bfs::root 33019544 --physical_memory 1073741824 \
	    --bfs_per_page ${BFs} --bf_mode use ${COMMON} >> bfs_${SUFFIX} 2>&1
        done
      done
    done
  done
