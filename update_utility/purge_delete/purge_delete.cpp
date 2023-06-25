#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cassert>
#include <iostream>
#include <vector>
#include <bitset>
#include <string>
#include <set>
#include <numeric>
#include <algorithm>
#include <random>
#include <chrono>

using namespace std;

struct simple_edge {
	unsigned int src;
	unsigned int dst;
};

int main(int argc, char** argv) {
	if(argc < 3){
		printf("./delete_edge <binary edge list> <output binary edge list>\n");
		exit(1);
	}

        FILE *bin_edge_fp = fopen(argv[1], "r");
        if(bin_edge_fp == NULL) {printf("binary edge file error\n"); exit(1);}
        fseeko(bin_edge_fp, 0L, SEEK_END);
        assert(ftello(bin_edge_fp) % 8 == 0);
        unsigned long long num_edge = ftello(bin_edge_fp) / 8;
        fseeko(bin_edge_fp, 0L, SEEK_SET);
        printf("num edge: %lld\n", num_edge);

	FILE *out_fp = fopen(argv[2], "w");
	if(out_fp == NULL) {printf("output edge file error\n"); exit(1);}

        struct simple_edge one_edge;
	for(unsigned long long i = 0; i < num_edge; ++i){
                int ret = fread(&one_edge, sizeof(struct simple_edge), 1, bin_edge_fp);
		assert(ret != 0);
                if(one_edge.src != 0xFFFFFFFF || one_edge.dst != 0xFFFFFFFF){
                        fwrite(&one_edge, sizeof(struct simple_edge), 1, out_fp);		
		}
	}
        fclose(out_fp);
        fclose(bin_edge_fp);
	return 0;
}

