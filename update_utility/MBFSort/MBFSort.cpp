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
#include <boost/dynamic_bitset.hpp>
#include "external_sort/external_sort.hpp"
#include <immintrin.h>

#define HASH_IN_BF 4

using namespace std;
//using namespace boost;

struct hash_all{ // (unsigned int) -1 is a special value
    unsigned short hash_val[4];
};

struct simple_edge {
	static unsigned int bloom_bits;
	static unsigned int bloom_set_bits;
	unsigned int src;
	unsigned int dst;
};

bool operator==(const struct hash_all& lhs, const struct hash_all& rhs){
	if(lhs.hash_val[0] == rhs.hash_val[0] && lhs.hash_val[1] == rhs.hash_val[1] &&
	lhs.hash_val[2] == rhs.hash_val[2] && lhs.hash_val[3] == rhs.hash_val[3])
		return true;
	return false;
}

bool operator!=(const struct hash_all& lhs, const struct hash_all &rhs){
	return !operator==(lhs, rhs);
}

inline struct hash_all bf_compute(unsigned int src, unsigned int bloom_set_bits){
    struct hash_all output;
    unsigned long long two_bf_mask = (((1 << bloom_set_bits) - 1) << 16) + ((1 << bloom_set_bits) - 1);
    unsigned long long tmp_hash = _mm_crc32_u64(0, src);
    unsigned long long hash1 = (tmp_hash & two_bf_mask);
    unsigned long long hash2 = (_mm_crc32_u64(tmp_hash, src) & two_bf_mask);
    output.hash_val[0] = hash1 & 0xFFFF;
    output.hash_val[1] = hash1 >> 16;
    output.hash_val[2] = hash2 & 0xFFFF;
    output.hash_val[3] = hash2 >> 16;
    return output;
}
unsigned int simple_edge::bloom_bits = 1u;
unsigned int simple_edge::bloom_set_bits = 1u;

bool operator<(const struct simple_edge& A, const struct simple_edge& B){
	struct hash_all hashA = bf_compute(A.src, simple_edge::bloom_set_bits);
	struct hash_all hashB = bf_compute(B.src, simple_edge::bloom_set_bits);
	for(unsigned int i = 1; i <= simple_edge::bloom_set_bits; ++i){
		struct hash_all com_hashA = hashA;
		struct hash_all com_hashB = hashB;
		for(int j = 0; j < HASH_IN_BF; ++j){
			com_hashA.hash_val[j] &= ((1 << i) - 1);
			com_hashB.hash_val[j] &= ((1 << i) - 1);
		}
		sort(com_hashA.hash_val, com_hashA.hash_val + HASH_IN_BF);
		sort(com_hashB.hash_val, com_hashB.hash_val + HASH_IN_BF);
		for(int j = 0; j < HASH_IN_BF; ++j){
			if(com_hashA.hash_val[j] < com_hashB.hash_val[j]){
				return true;
			}else if(com_hashA.hash_val[j] > com_hashB.hash_val[j]){
				return false;
			}
		}
	}
	if (A.src < B.src) return true;
	if (A.src > B.src) return false;
	if (A.dst < B.dst) return true;
	return false;
}

int main(int argc, char** argv) {
	if(argc < 3){
		printf("./c_reorg3 <binary edge list> <bloom bits> <output binary edge list>\n");
		exit(1);
	}

	int bloom_bits = atoi(argv[2]);
	boost::dynamic_bitset<> tmp_bitset(16, bloom_bits);
	assert(tmp_bitset.count() == 1);
    simple_edge::bloom_bits = bloom_bits;
	simple_edge::bloom_set_bits = (unsigned int) tmp_bitset.find_first();
	cout << "bloom_set_bit: " << simple_edge::bloom_set_bits << endl;

	external_sort::SplitParams sp;
    external_sort::MergeParams mp;
    sp.mem.blocks = 8;
    sp.mem.size = 5000;
    sp.mem.unit = external_sort::MB;
    mp.mem.blocks = 4;
    mp.mem.size = 1000;
    mp.mem.unit = external_sort::MB;
    sp.spl.ifile = string(argv[1]);
    mp.mrg.ofile = string(argv[3]);

    using ValueType = struct simple_edge;
    external_sort::sort<ValueType>(sp, mp); // run external sort

    if (sp.err.none && mp.err.none) { std::cout << "File sorted successfully!" << std::endl; } 
	else {
        std::cout << "External sort failed!" << std::endl;
        if (sp.err) { std::cout << "Split failed: " << sp.err.msg() << std::endl; } 
		else { std::cout << "Merge failed: " << mp.err.msg() << std::endl; }
		exit(1);
    }

	return 0;
}

