
# MBFGraph: An SSD-Based Analytics System for Evolving Graphs (SC'23)

This is external graph system designed for Solid-State Disks (SSDs). That is, MBFGraph can use very small amount of memory to process very large graphs (e.g., processing 475GB graph with only 4GB memory). Specifically, MBFGraph adopts Millions of Bloom Filters simultaneously as approximate indice to efficiently filter out unnecessary read data and significantly reduce graph analysis time.

## Software Library Dependency: 
    C++ boost library
  
    libaio

## OS environment:
    Ubuntu 20.04
  
    Dependency packages: libboost-all-dev libaio-dev

## Hardware requirement:
  x86 CPU with AVX2 instruction support
  
    You can check /proc/cpuinfo to see whether you CPU support such instructions.
    Note that such instruction extension should already be widely supported.

  SSDs with good random performance
  
    Most SSDs should be able to provide decent random performance. However, some QLC-based ultra-high-capacity SSDs may provide very bad random performnace.

## Test Hardware environment:
    CPU: i7-4790
  
    Storage: Samsung EVO 870 2TB

## Build MBFGraph:
    make -j

    Troubleshooting: If you encounter any makefile error, please remove anything under object_files/.

## Usage:
    Note that all different graph analyses, by default, are only for BFS. To enable PageRank and CC, please uncomment some lines in the run_###.sh.
    Also note that, to reduce repeatly bloom filter (BF) files generation time, the script may generate BF file via degree_cnt analysis. As a result, BF files can be reused across different graph analyses.
    
    Configuration: 
     Number of CPUs: Every run_##.sh provides an environment variable, PROCE, to adjust the number of threads executing during graph analyses.
     Threshold on whether performing MBF query: Every run_##.sh provides an environment variable, MAX, to decide whether MBF query has to be performed. If MAX is 1, it indicates that MBF query is not executed, so MBFGraph is actually the baseline x-stream.
### Static Twitter Graph:
    ./download_twitter.sh
  
    ./run_twitter.sh  # This is MBFGraph-optimized version of BFS on Twitter graph.  
  
    ./run_twitter_baseline.sh # This is baseline (X-stream) version of BFS on Twitter graph.

### Static Random Graph:
    ./download_random.sh

    ./run_random.sh # This is MBFGraph-optimized version of BFS on Random (hello) graph.

    ./run_random_baseline.sh # This is baseline (X-stream) version of BFS on Random (hello) graph.

### Static Web Graph:
    ./download_web.sh

    ./run_web.sh # This is MBFGraph-optimized version of BFS on Web graph. To test the baseline version, please change MAX environment from 270000 to 1.

### Static Web_large graph:
    ./download_web_large.sh # We're still working on uploading entire (475GB) graph to Zenodo.

    ./run_web.sh # This is MBFGraph-optimized version of BFS on Web_large graph. To test the baseline version, please change MAX environment from 270000 to 1.

### Dynamically adding edges on Random graph:
    ./download_random.sh

    ./download_add.sh # three different datasets (1, 1M, 10M) are downloaded. 

    ./run_random_update.sh # This is MBFGraph-optimized version of BFS on evolving Random graph. To test the baseline version, please change MAX environment from 270000 to 1.

    Note that the edge-adding dataset can be changed to 1 or 1M edges. (default: 10M per round)
### Dynamically deleting edges on Twitter graph:
    ./download_twitter.sh

    ./run_delete_twitter.sh # This is MBFGraph-optimized version of BFS on evolving Twitter graph without purging. To test the baseline version, please change MAX environment from 270000 to 1.

    ./run_purge_delete_twitter.sh # This is MBFGraph-optimized version of BFS on evolving Twitter graph with purging. To test the baseline version, please change MAX environment from 270000 to 1.

    Note that delete_edge and purge_delete utilities are provided under update_utility/.
    
### MBFSort:
    This utility is provided under update_utility.
    Usage: ./MBFSort <input edge (graph) file> <bloom filter bits> <output edge (graph) file>
    Example: ./MBFSort input_twitter 8 output_twitter # 8 means 256 (2^8) bits.

## Dataset and Docker image
Dataset: [https://doi.org/10.5281/zenodo.8076831](https://doi.org/10.5281/zenodo.8076831)

Dataset: [https://doi.org/10.5281/zenodo.8088562](https://doi.org/10.5281/zenodo.8088562)

Docker image: [https://doi.org/10.5281/zenodo.8080103](https://doi.org/10.5281/zenodo.8080103)
