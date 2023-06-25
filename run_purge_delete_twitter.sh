#!/bin/bash -v

BASE_DIR=/media/liu/test_1TB/
DATA=purge
DATA2=purge2
MAX=600000
#MAX=1
KERNEL=CPU
PROCE=8


for IN_DIR in my_sort #normal_sort
do
  for index in 15 #{15..0} #15
  do
    for opt_bit in 256 #8 16 32 64 128 256 512 1024 2048
    do
      rm tmp_${DATA}
      rm ${DATA}
      cp twitter ${DATA}
      cp twitter.ini ${DATA}.ini
      sync
      for PAGE in 2048 #512 1024 4096 #8192 16384
      do
        BFs=$((PAGE / 8))
	BFs2=$((PAGE / 16))
        for BIT in 256 #128 512 1024 2048
        do
          SUFFIX=${DATA}_bf${BIT}_${IN_DIR}_o${opt_bit}_${index}_pg${BFs}
	  
          COMMON1=" --measure_scatter_gather --heartbeat -a -p ${PROCE} --page_size ${PAGE} --bits_per_BF ${BIT} "
          COMMON2=" --chip_v_once 4 --max_check_v ${MAX} --kernel ${KERNEL}"
          COMMON=${COMMON1}${COMMON2}

	  DEL_EDGE=0
          for part in {0..99} 
	  do
	    BASE_DELETE=10000000
	    START_EDGE=$DEL_EDGE
	    DEL_EDGE=$((DEL_EDGE + BASE_DELETE))
	    echo start ${START_EDGE} end ${DEL_EDGE}
	    ./delete_page ${DATA} ${START_EDGE} ${DEL_EDGE} 1
	    mv ${DATA} tmp_${DATA}
	    ./purge_delete tmp_${DATA} ${DATA}
            
	    for MEM in 1073741824 536870912 268435456
	    do
	      rm degree_${MEM}_${SUFFIX}_pd${part}*
	      ./bin/benchmark_driver -g ${DATA} -b degree_cnt --physical_memory ${MEM} \
                --bfs_per_page ${BFs} --bf_mode create ${COMMON} >> degree_${MEM}_${SUFFIX}_pd${part} 2>&1
            done

	    #rm sssp_${SUFFIX}_pd${part}*
            #./bin/benchmark_driver -g ${DATA2} -b sssp --sssp::source 3 --physical_memory 1073741824 \
	    #  --bfs_per_page ${BFs2} --bf_mode use ${COMMON} >> sssp_${SUFFIX}_pd${part} 2>&1
          
            rm bfs_${SUFFIX}_pd${part}*
            ./bin/benchmark_driver -g ${DATA} -b bfs --bfs::root 3 --physical_memory 1073741824 \
	     --bfs_per_page ${BFs} --bf_mode use ${COMMON} >> bfs_${SUFFIX}_pd${part} 2>&1

            #rm cc_${SUFFIX}_pd${part}*
            #./bin/benchmark_driver -g ${DATA} -b cc --physical_memory 1073741824 \
	    #  --bfs_per_page ${BFs} --bf_mode use ${COMMON} >> cc_${SUFFIX}_pd${part} 2>&1
  
            #rm page_${SUFFIX}_pd${part}*
            #./bin/benchmark_driver -g ${DATA} -b pagerank_ddf --pagerank::niters 30 --physical_memory 1073741824 \
	    #  --bfs_per_page ${BFs} --bf_mode use ${COMMON} >> page_${SUFFIX}_pd${part} 2>&1

	    rm ${DATA}
	    mv tmp_${DATA} ${DATA}
	  done

        done
      done
      rm ${DATA}
    done
  done
done
