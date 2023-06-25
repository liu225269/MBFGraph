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
	if(argc < 5){
		printf("./delete_edge <binary edge list> <start deleted> <number of deleted edges> <deleted percentage>\n");
		exit(1);
	}

        FILE *bin_edge_fp = fopen(argv[1], "r+");
        if(bin_edge_fp == NULL) {printf("binary edge file error\n"); exit(1);}
        fseeko(bin_edge_fp, 0L, SEEK_END);
        assert(ftello(bin_edge_fp) % 8 == 0);
        unsigned long long num_edge = ftello(bin_edge_fp) / 8;
        fseeko(bin_edge_fp, 0L, SEEK_SET);
        printf("num edge: %lld\n", num_edge);

        unsigned int start_edge = atoi(argv[2]);
	unsigned int deleted_edge = atoi(argv[3]);
	unsigned int deleted_percentage = atoi(argv[4]);
	assert(0 < deleted_percentage && deleted_percentage < 100);

	if(deleted_edge == 0) deleted_edge = num_edge * deleted_percentage / 100;
	//unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
        std::default_random_engine generator;

        std::uniform_int_distribution<uint64_t> distribution(
	    std::numeric_limits<std::uint64_t>::min(),
            std::numeric_limits<std::uint64_t>::max());

        vector<bool> deleted_edges(num_edge, false);

        struct simple_edge deleted_simple_edge{(unsigned int)-1, (unsigned int)-1};
	for(unsigned int i = 0; i < deleted_edge; ++i){
                unsigned long long position = (distribution(generator) % num_edge);
		if(start_edge < i) deleted_edges[position] = true;
		//cout << "delete pos: " << (position / 8) << endl;
	}
	for(unsigned long long i = 0; i < num_edge; ++i){
		if(deleted_edges[i] == true){
			fseeko(bin_edge_fp, i * 8, SEEK_SET);
			int ret = fwrite(&deleted_simple_edge, sizeof(struct simple_edge), 1, bin_edge_fp);
			//cout << "ret " << ret << endl; 
			//if(i % 1000 == (1000 - 1)) cout << i << " deleted." << endl;
		}
	}
	cout << deleted_edge << " done." << endl;
	fclose(bin_edge_fp);
	return 0;
}

