
MBFGraph: An SSD-Based Analytics System for Evolving Graphs 

This is external graph system designed for Solid-State Disks (SSDs). That is, MBFGraph can use very small amount of memory to process very large graphs (e.g., processing 475GB graph with only 4GB memory). Specifically, MBFGraph adopts Millions of Bloom Filters simultaneously as approximate indice to efficiently filter out unnecessary read data and significantly reduce graph analysis time.

Software Library Dependency: 
  C++ boost library
  libaio

OS environment:
  Ubuntu 20.04
  Dependency packages: libboost-all-dev libaio-dev

Hardware requirement:
  x86 CPU with AVX2 instruction support
    You can check /proc/cpuinfo to see whether you CPU support such instructions.
    Note that such instruction extension should already be widely supported.

  SSDs with good random performance
    Most SSDs should be able to provide decent random performance. However, some QLC-based ultra-high-capacity SSDs may provide very bad random performnace.

Test Hardware environment:
  CPU: i7-4790
  Storage: Samsung EVO 870 2TB

Usage:
unzip test.zip
make
./run_test.sh
