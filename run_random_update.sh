#!/bin/bash -v

BASE_DIR=.
DATA=add
DATA2=add2
#MAX=131000
MAX=600000
#MAX=1
KERNEL=CPU
PROCE=8
BASE_EDGE=2147483648
BASE_VERTEX=67108864


  rm ${DATA}
  cp ${BASE_DIR}/hello ${DATA}
  sync
  for PAGE in 2048 #512 1024 4096 8192 #16384
  do
    BFs=$((PAGE / 8))
    for BIT in 256 #128 512 1024 #2048
    do
      SUFFIX=${DATA}_bf${BIT}_${IN_DIR}_pg${BFs}

      COMMON1=" --measure_scatter_gather --heartbeat -a -p ${PROCE} --page_size ${PAGE} --bits_per_BF ${BIT} "
      COMMON2=" --chip_v_once 4 --max_check_v ${MAX} --kernel ${KERNEL}"
      COMMON=${COMMON1}${COMMON2}
         
      echo -e "[graph]\nname=${DATA}\ntype=2\nvertices=${BASE_VERTEX}" > ${DATA}.ini
      echo -e "edges=${BASE_EDGE}" >> ${DATA}.ini
      rm degree_*_${SUFFIX}
      for MEM in 1073741824 #536870912 #268435456 #134217728 67108864 1073741824 
      do
        ./bin/benchmark_driver -g ${DATA} -b degree_cnt --physical_memory ${MEM} \
          --bfs_per_page ${BFs} --bf_mode create ${COMMON} >> degree_${MEM}_${SUFFIX} 2>&1
      done 
      
      EDGE=$BASE_EDGE
      for part in {000..099}
      do
        cat ${BASE_DIR}/sort_part_10M/part_${part} >> ${DATA}
        #cat ${BASE_DIR}/sort_part/part_${part} >> ${DATA}
        #cat ${BASE_DIR}/sort_part_1/part_${part} >> ${DATA}
        echo -e "[graph]\nname=${DATA}\ntype=2\nvertices=${BASE_VERTEX}" > ${DATA}.ini
	EDGE=$((EDGE + 10485760))
	#EDGE=$((EDGE + 1048576))
	#EDGE=$((EDGE + 1))
	echo -e "edges=${EDGE}" >> ${DATA}.ini
        
        #rm cc_${SUFFIX}
        #./bin/benchmark_driver -g ${DATA} -b cc --physical_memory 1073741824 \
        #  --bfs_per_page ${BFs} --bf_mode update ${COMMON} >> cc_u${part}_${SUFFIX} 2>&1
  
        #rm page_${SUFFIX}
        #./bin/benchmark_driver -g ${DATA} -b pagerank_ddf --pagerank::niters 30 --physical_memory 1610612736 \
        #  --bfs_per_page ${BFs} --bf_mode update ${COMMON} >> page_u${part}_${SUFFIX} 2>&1

        rm bfs_u${part}_${SUFFIX} 
	./bin/benchmark_driver -g ${DATA} -b bfs --bfs::root 33019544 --physical_memory 1073741824 \
          --bfs_per_page ${BFs} --bf_mode update ${COMMON} >> bfs_u${part}_${SUFFIX} 2>&1
      done
    done
  done
  rm ${DATA}
