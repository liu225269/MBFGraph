#!/bin/bash -v

DATA=twitter
DATA2=twitter2
#MAX=600000
MAX=1
KERNEL=CPU
PROCE=8


  for index in 15 #{15..0} #15
  do
    for opt_bit in 256 #8 16 32 64 128 256 512 1024 2048
    do
      for PAGE in 2048 #512 1024 4096 #8192 16384
      do
        BFs=$((PAGE / 8))
	BFs2=$((PAGE / 16))
        for BIT in 256 #128 512 1024 2048
        do
          SUFFIX=${DATA}_bf${BIT}_o${opt_bit}_${index}_pg${BFs}_base
	  
          COMMON1=" --measure_scatter_gather --heartbeat -a -p ${PROCE} --page_size ${PAGE} --bits_per_BF ${BIT} "
          COMMON2=" --chip_v_once 4 --max_check_v ${MAX} --kernel ${KERNEL}"
          COMMON=${COMMON1}${COMMON2}

	 rm degree_*_${SUFFIX}
         for MEM in 1073741824 536870912 #268435456 134217728 67108864 33554432 
         do
           ./bin/benchmark_driver -g ${DATA} -b degree_cnt --physical_memory ${MEM} \
             --bfs_per_page ${BFs} --bf_mode create ${COMMON} >> degree_${MEM}_${SUFFIX} 2>&1
         done

	  #rm sssp_${SUFFIX}_*
	  #for MEM in 1073741824 #{67108864..1073741824..67108864}
	  #do
          #  ./bin/benchmark_driver -g ${DATA2} -b sssp --sssp::source 3 --physical_memory ${MEM} \
	  #    --bfs_per_page ${BFs2} --bf_mode use ${COMMON} >> sssp_${SUFFIX}_${MEM} 2>&1
          #done
          echo start
          rm bfs_${SUFFIX}_*
          for MEM in 1073741824 #{67108864..1073741824..67108864}
	  do
            ./bin/benchmark_driver -g ${DATA} -b bfs --bfs::root 3 --physical_memory ${MEM} \
	     --bfs_per_page ${BFs} --bf_mode use ${COMMON} >> bfs_${SUFFIX}_${MEM} 2>&1
	  done

          #rm page_${SUFFIX}_*
          #for MEM in 1073741824 #1610612736 #{1073741824..1610612736..67108864}
	  #do
          #  ./bin/benchmark_driver -g ${DATA} -b pagerank_ddf --pagerank::niters 30 --physical_memory ${MEM} \
	  #    --bfs_per_page ${BFs} --bf_mode use ${COMMON} >> page_${SUFFIX}_${MEM} 2>&1
          #done

          #rm cc_${SUFFIX}_*
          #for MEM in 1073741824 #134217728 #{67108864..1073741824..67108864}
	  #do
          #./bin/benchmark_driver -g ${DATA} -b cc --physical_memory ${MEM} \
	  #  --bfs_per_page ${BFs} --bf_mode use ${COMMON} >> cc_${SUFFIX}_${MEM} 2>&1
	  #done
        done
      done
    done
  done
