/*
 * X-Stream
 *
 * Copyright 2013 Operating Systems Laboratory EPFL
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _SG_DRIVER_
#define _SG_DRIVER_
#include <immintrin.h>
#include <sys/resource.h>
#include <sys/time.h>

#include "bitshuffle_core.h"
#include "x-lib.hpp"

// Implement a wrapper for simpler graph algorithms that alternate between
// synchronously gathering updates and synchronously scattering them along edges

#define FORCE_INLINE inline __attribute__((always_inline))

inline uint32_t rotl32(uint32_t x, int8_t r) {
  return (x << r) | (x >> (32 - r));
}

#define ROTL32(x, y) rotl32(x, y)

FORCE_INLINE uint32_t getblock32(const uint32_t *p, int i) { return p[i]; }

FORCE_INLINE uint32_t fmix32(uint32_t h) {
  h ^= h >> 16;
  h *= 0x85ebca6b;
  h ^= h >> 13;
  h *= 0xc2b2ae35;
  h ^= h >> 16;

  return h;
}

FORCE_INLINE void MurmurHash3_x86_32(const void *key, uint32_t seed,
                                     void *out) {
  const uint8_t *data = (const uint8_t *)key;
  const int nblocks = 4 / 4;

  uint32_t h1 = seed;

  const uint32_t c1 = 0xcc9e2d51;
  const uint32_t c2 = 0x1b873593;

  //----------
  // body

  const uint32_t *blocks = (const uint32_t *)(data + nblocks * 4);

  for (int i = -nblocks; i; i++) {
    uint32_t k1 = getblock32(blocks, i);

    k1 *= c1;
    k1 = ROTL32(k1, 15);
    k1 *= c2;

    h1 ^= k1;
    h1 = ROTL32(h1, 13);
    h1 = h1 * 5 + 0xe6546b64;
  }

  //----------
  // tail

  const uint8_t *tail = (const uint8_t *)(data + nblocks * 4);

  uint32_t k1 = 0;

  switch (4 & 3) {
    case 3:
      k1 ^= tail[2] << 16;
    case 2:
      k1 ^= tail[1] << 8;
    case 1:
      k1 ^= tail[0];
      k1 *= c1;
      k1 = ROTL32(k1, 15);
      k1 *= c2;
      h1 ^= k1;
  };
  //----------
  // finalization

  h1 ^= 4;

  h1 = fmix32(h1);

  *(uint32_t *)out = h1;
}

static void bitset_generate_table(unsigned char count_table[256]) {
  for (int i = 0; i < 256; ++i) {
    for (int j = 0; j < 8; ++j) {
      if ((i & (1 << j)) != 0) count_table[i]++;
    }
  }
}

static unsigned int bitset_count(
    std::vector<unsigned char,
                boost::alignment::aligned_allocator<unsigned char, 4096> >
        &test,
    unsigned char count_table[256]) {
  unsigned int sum = 0;
  for (unsigned int i = 0; i < test.size(); ++i) {
    sum += count_table[test[i]];
  }
  return sum;
}

static unsigned int bitset_size(
    std::vector<unsigned char,
                boost::alignment::aligned_allocator<unsigned char, 4096> >
        &test) {
  return test.size() * 8;
}

static void bitset_clear(
    std::vector<unsigned char,
                boost::alignment::aligned_allocator<unsigned char, 4096> >
        &test) {
  for (unsigned int i = 0; i < test.size(); ++i) {
    test[i] = 0;
  }
}

namespace algorithm {
const unsigned long phase_edge_split = 0;
const unsigned long superphase_begin = 1;
const unsigned long phase_gather = 2;
const unsigned long phase_BF_checking_1 = 3;
const unsigned long phase_BF_checking_2 = 4;
const unsigned long phase_scatter = 5;
const unsigned long phase_post_scatter = 6;
const unsigned long phase_terminate = 7;

struct sg_pcpu : public per_processor_data {
  unsigned long processor_id;
  bool i_vote_to_stop;
  static sg_pcpu **per_cpu_array;
  static x_barrier *sync;
  // Stats
  unsigned long update_bytes_out;
  unsigned long update_bytes_in;
  unsigned long edge_bytes_streamed;
  unsigned long partitions_processed;
  unsigned long previous_pos;
  //
  static x_lib::filter *scatter_filter;
  static rtc_clock pc_clock;
  bool activate_partition_for_scatter;

  /* begin work specs. */
  static unsigned long bsp_phase;
  static unsigned long current_step;
  static bool do_algo_reduce;
  /* end work specs. */

  static int partition;
  static unsigned int all_BF_cnt;
  static std::vector<vertex_t,
                     boost::alignment::aligned_allocator<vertex_t, 4096> >
      all_BF_vertex;
  static vertex_t **BF_vertex;
  static int *BF_vertex_cnt;
  static int *BF_vertex_nr;
  static std::vector<unsigned char,
                     boost::alignment::aligned_allocator<unsigned char, 4096> >
      BF_bitset;
  static std::vector<unsigned char,
                     boost::alignment::aligned_allocator<unsigned char, 4096> >
      ver_buf_chg;
  static unsigned char count_table[256];

  static per_processor_data **algo_pcpu_array;
  per_processor_data *algo_pcpu;

  bool reduce(per_processor_data **per_cpu_array, unsigned long processors) {
    if (algo_pcpu_array[0] != NULL && do_algo_reduce) {
      return algo_pcpu_array[0]->reduce(algo_pcpu_array, processors);
    } else {
      return false;  // Should be don't care
    }
  }
} __attribute__((__aligned__(64)));

template <typename A, typename F>
class scatter_gather {
  sg_pcpu **pcpu_array;
  bool heartbeat;
  struct timeval prev, cur;
  bool measure_scatter_gather;
  x_lib::streamIO<scatter_gather> *graph_storage;
  unsigned long vertex_stream;
  unsigned long edge_stream;
  unsigned long updates0_stream;
  unsigned long updates1_stream;
  unsigned long init_stream;
  unsigned long bloom_stream;
  rtc_clock wall_clock;
  rtc_clock setup_time;
  rtc_clock state_iter_cost;
  rtc_clock scatter_cost;
  rtc_clock gather_cost;

 public:
  scatter_gather();
  void partition_super_partition(x_lib::streamIO<scatter_gather> *graph_storage,
                                 std::string efile, int BITS_PER_BF);
  void bloom_filter_calculation(unsigned char *edge_buf,
                                unsigned char *bloom_in,
                                unsigned char *bloom_out, FILE *bf_fp,
                                unsigned int read_page,
                                unsigned int BFS_PER_PAGE,
                                unsigned int BITS_PER_BF, bool is_verify);
  unsigned long long file_size(FILE *fp, unsigned int unit_size);
  unsigned long long sanity_check(FILE *edge_fp, std::vector<FILE *> edge_fpp,
                                  std::vector<FILE *> bf_fpp,
                                  std::vector<unsigned long long> &num_edges,
                                  unsigned long long *start_edge,
                                  int BITS_PER_BF, int BFS_PER_PAGE,
                                  int SIMD_LEN);
  void edge_distribution(unsigned long super_partitions, FILE *edge_fp,
                         std::vector<FILE *> edge_fpp,
                         std::vector<FILE *> bf_fpp,
                         unsigned long long num_edge,
                         std::vector<unsigned long long> num_edges,
                         unsigned long long start_edge,
                         unsigned int BFS_PER_PAGE, unsigned int BITS_PER_BF,
                         int SIMD_LEN, bool is_verify);
  static void partition_pre_callback(unsigned long super_partition,
                                     unsigned long partition,
                                     per_processor_data *cpu_state);
  static void partition_callback(x_lib::stream_callback_state *state);
  static void partition_post_callback(unsigned long super_partition,
                                      unsigned long partition,
                                      per_processor_data *cpu_state);
  void operator()();

  static unsigned long max_streams() {
    return 6;  // vertices, edges, init_edges, updates0, updates1, bloom
  }

  static unsigned long max_buffers() { return 4; }

  static unsigned long vertex_state_bytes() { return A::vertex_state_bytes(); }

  static unsigned long vertex_stream_buffer_bytes() {
    return A::split_size_bytes() + F::split_size_bytes();
  }

  static void state_iter_callback(unsigned long superp, unsigned long partition,
                                  unsigned long index, unsigned char *vertex,
                                  per_processor_data *cpu_state, unsigned char *base_vertex) {
    unsigned long global_index =
        x_lib::configuration::map_inverse(superp, partition, index);
    sg_pcpu *pcpu = static_cast<sg_pcpu *>(cpu_state);
    if (sg_pcpu::current_step == superphase_begin ||
        sg_pcpu::current_step == phase_post_scatter) {
      bool will_scatter = A::init(vertex, global_index, sg_pcpu::bsp_phase,
                                  sg_pcpu::algo_pcpu_array[pcpu->processor_id]);
      if (will_scatter) {
        sg_pcpu::scatter_filter->q(partition);
      }
    } else if(sg_pcpu::current_step == phase_scatter) { 
      if (A::check_reset_changed(vertex, global_index, sg_pcpu::bsp_phase)){
        unsigned int pos = (vertex - base_vertex) / 512;
        unsigned int t_pos = (vertex + A::vertex_state_bytes() - 1 - base_vertex) / 512;
        if(pcpu->previous_pos != pos || pcpu->previous_pos != t_pos){
          __sync_fetch_and_or(&sg_pcpu::ver_buf_chg[pos / 8], (1 << (pos % 8)));
          __sync_fetch_and_or(&sg_pcpu::ver_buf_chg[t_pos / 8], (1 << (t_pos % 8)));
	  //if(pos != t_pos) printf("$%u,%u$ ", pos, t_pos);
          pcpu->previous_pos = pos;
        }
      }
    } else if (sg_pcpu::current_step == phase_BF_checking_1) {
      if (A::check_bsp(vertex, global_index, sg_pcpu::bsp_phase,
                       sg_pcpu::algo_pcpu_array[pcpu->processor_id])) {
        sg_pcpu::BF_vertex_nr[partition]++;
      }
    } else if(sg_pcpu::current_step == phase_BF_checking_2) {
      if (A::check_bsp(vertex, global_index, sg_pcpu::bsp_phase,
                       sg_pcpu::algo_pcpu_array[pcpu->processor_id])) {
        sg_pcpu::BF_vertex[partition][sg_pcpu::BF_vertex_cnt[partition]] =
            global_index;
        sg_pcpu::BF_vertex_cnt[partition]++;
      }
    } else
      BOOST_ASSERT_MSG(0, "Do not support this step in state iteration");
  }

  static per_processor_data *create_per_processor_data(
      unsigned long processor_id) {
    return sg_pcpu::per_cpu_array[processor_id];
  }

  static void do_cpu_callback(per_processor_data *cpu_state) {
    sg_pcpu *cpu = static_cast<sg_pcpu *>(cpu_state);
    if (sg_pcpu::current_step == superphase_begin) {
      cpu->i_vote_to_stop = true;
    } else if (sg_pcpu::current_step == phase_scatter) {
      cpu->previous_pos = (unsigned long) -1;
    } else if (sg_pcpu::current_step == phase_post_scatter) {
      sg_pcpu::scatter_filter->done(cpu->processor_id);
    } else if (sg_pcpu::current_step == phase_terminate) {
      BOOST_LOG_TRIVIAL(info)
          << "CORE::PARTITIONS_PROCESSED " << cpu->partitions_processed;
      BOOST_LOG_TRIVIAL(info)
          << "CORE::BYTES::EDGES_STREAMED " << cpu->edge_bytes_streamed;
      BOOST_LOG_TRIVIAL(info)
          << "CORE::BYTES::UPDATES_OUT " << cpu->update_bytes_out;
      BOOST_LOG_TRIVIAL(info)
          << "CORE::BYTES::UPDATES_IN " << cpu->update_bytes_in;
    }
  }
};

template <typename A, typename F>
void scatter_gather<A, F>::bloom_filter_calculation(
    unsigned char *edge_buf, unsigned char *bloom_in, unsigned char *bloom_out,
    FILE *bf_fp, unsigned int read_edge, unsigned int BFS_PER_PAGE,
    unsigned int BITS_PER_BF, bool is_verify) {
  unsigned int SIMD_LEN = 256;
  unsigned int bf_buf_size = SIMD_LEN * BITS_PER_BF / 8;
  unsigned int two_bf_mask = ((BITS_PER_BF - 1) << 16) + (BITS_PER_BF - 1);
  for (unsigned int i = 0; i < bf_buf_size; i++) bloom_in[i] = 0;
  for (unsigned int i = 0; i < bf_buf_size; i++) bloom_out[i] = 0;

  for (unsigned int j = 0, m = 0; j < read_edge; j += BFS_PER_PAGE, m++) {
    unsigned long long *sub_bloom_filter =
        (unsigned long long *)&bloom_in[m * BITS_PER_BF / 8];
    // printf("%x\n", sub_bloom_filter);
    unsigned long edge_size = F::split_size_bytes();
    for (unsigned int k = j; k < std::min(read_edge, j + BFS_PER_PAGE); ++k) {
      int src = F::split_key(&edge_buf[k * edge_size], 0);
      unsigned int tmp_hash = _mm_crc32_u64(0, src);
      unsigned int hash1 = tmp_hash & two_bf_mask;
      unsigned int hash2 = (_mm_crc32_u64(tmp_hash, src) & two_bf_mask);
      unsigned int hashA = hash1 & 0xFFFF, hashB = hash1 >> 16;
      unsigned int hashC = hash2 & 0xFFFF, hashD = hash2 >> 16;
      sub_bloom_filter[hashA >> 6] |= (1ull << (hashA & 0x3F));
      sub_bloom_filter[hashB >> 6] |= (1ull << (hashB & 0x3F));
      sub_bloom_filter[hashC >> 6] |= (1ull << (hashC & 0x3F));
      sub_bloom_filter[hashD >> 6] |= (1ull << (hashD & 0x3F));
      // printf("%d: %llx %llx %llx %llx\n", k, sub_bloom_filter[0],
      // sub_bloom_filter[1], sub_bloom_filter[2], sub_bloom_filter[3]);
    }
  }
  if (bshuf_bitshuffle(bloom_in, bloom_out, SIMD_LEN, BITS_PER_BF / 8,
                       bf_buf_size) != bf_buf_size) {
    assert(0);
  }
  if (is_verify) {
    if (fread(bloom_in, 1, bf_buf_size, bf_fp) != bf_buf_size) assert(0);
    assert(memcmp(bloom_in, bloom_out, bf_buf_size) == 0);
  } else {
    if (fwrite(bloom_out, 1, bf_buf_size, bf_fp) != bf_buf_size) assert(0);
  }
}

template <typename A, typename F>
void scatter_gather<A, F>::edge_distribution(
    unsigned long super_partitions, FILE *edge_fp, std::vector<FILE *> edge_fpp,
    std::vector<FILE *> bf_fpp, unsigned long long num_edge,
    std::vector<unsigned long long> num_edges, unsigned long long start_edge,
    unsigned int BFS_PER_PAGE, unsigned int BITS_PER_BF, int SIMD_LEN,
    bool is_verify) {
  unsigned int edges_per_buf = BFS_PER_PAGE * SIMD_LEN;
  unsigned int edge_buf_size = edges_per_buf * F::split_size_bytes();
  unsigned int bf_buf_size = SIMD_LEN * BITS_PER_BF / 8;
  std::vector<unsigned char *> edge_bufs(super_partitions);
  std::vector<unsigned char *> bf_bufs(super_partitions),
      trans_bf_bufs(super_partitions);
  unsigned char *edge_buf = (unsigned char *)malloc(edge_buf_size);
  unsigned char *verify_edge_buf = (unsigned char *)malloc(edge_buf_size);
  assert(edge_buf != NULL);
  assert(verify_edge_buf != NULL);
  for (unsigned int i = 0; i < super_partitions; ++i) {
    edge_bufs[i] = (unsigned char *)malloc(edge_buf_size);
    bf_bufs[i] = (unsigned char *)malloc(BITS_PER_BF / 8 * SIMD_LEN);
    trans_bf_bufs[i] = (unsigned char *)malloc(BITS_PER_BF / 8 * SIMD_LEN);
  }
  if (super_partitions == 1) {
    if (start_edge != num_edge) {
      fseeko(bf_fpp[0], (start_edge / edges_per_buf) * bf_buf_size, SEEK_SET);
    }
  } else {
    for (unsigned int i = 0; i < super_partitions; ++i) {
      if (num_edges[i] != 0) {
        std::cout << "offset_adj: " << num_edges[i] << " (" << i << ")" << std::endl; 
        unsigned long long previous_edges = num_edges[i] % edges_per_buf;
        fseeko(edge_fpp[i],
               F::split_size_bytes() * (num_edges[i] - previous_edges),
               SEEK_SET);
        if (previous_edges != 0) {
          unsigned long long ret = fread(edge_bufs[i], F::split_size_bytes(), previous_edges,
                edge_fpp[i]);
          BOOST_ASSERT_MSG(ret == previous_edges * F::split_size_bytes(),
            "read amount is incorrect");
          fseeko(edge_fpp[i],
               F::split_size_bytes() * (num_edges[i] - previous_edges),
               SEEK_SET);
        }
        fseeko(bf_fpp[i], (num_edges[i] / edges_per_buf) * bf_buf_size,
               SEEK_SET);
        num_edges[i] = previous_edges;
        std::cout << num_edges[i] << " " << ftello(edge_fpp[i]) << " ";
        std::cout << ftello(bf_fpp[i]) << std::endl;
      }
    }
  }
  //std::cout << "bf pos: " << ftello(bf_fpp[0]) << " " << num_edges[0] << " "
  //          << start_edge << std::endl;

  unsigned long long i = start_edge;
  fseeko(edge_fp, i * F::split_size_bytes(), SEEK_SET);
  while (i < num_edge) {
    unsigned int read_edge =
        std::min((unsigned long long)(edges_per_buf), num_edge - i);
    if (fread(edge_buf, F::split_size_bytes(), read_edge, edge_fp) !=
        read_edge) {
      assert(0);
    }
    for (unsigned int j = 0; j < read_edge; ++j) {
      unsigned long superp = x_lib::map_super_partition_wrap::map(
          F::split_key(&edge_buf[j * F::split_size_bytes()], 0));
      assert(F::split_key(&edge_buf[j * F::split_size_bytes()], 0) != 0xFFFFFFFF);
      memcpy(&edge_bufs[superp][num_edges[superp] * F::split_size_bytes()],
             &edge_buf[j * F::split_size_bytes()], F::split_size_bytes());
      ++num_edges[superp];
      if (num_edges[superp] == BFS_PER_PAGE * SIMD_LEN) {
        bloom_filter_calculation(edge_bufs[superp], bf_bufs[superp],
                                 trans_bf_bufs[superp], bf_fpp[superp],
                                 num_edges[superp], BFS_PER_PAGE, BITS_PER_BF,
                                 is_verify);
        if (super_partitions > 1) {
          if (is_verify) {
            if (fread(verify_edge_buf, F::split_size_bytes(), edges_per_buf,
                      edge_fpp[superp]) != edges_per_buf)
              assert(0);
            assert(memcmp(verify_edge_buf, edge_bufs[superp], edge_buf_size) ==
                   0);
          } else {
            if (fwrite(edge_bufs[superp], F::split_size_bytes(), edges_per_buf,
                       edge_fpp[superp]) != edges_per_buf)
              assert(0);
          }
        }
        num_edges[superp] = 0;
      }
    }
    i += read_edge;
  }
  //std::cout << "bf pos: " << ftello(bf_fpp[0]) << " " << num_edges[0] << " "
  //          << start_edge << std::endl;
  for (unsigned int i = 0; i < super_partitions; ++i) {
    if (num_edges[i] != 0) {
      bloom_filter_calculation(edge_bufs[i], bf_bufs[i], trans_bf_bufs[i],
                               bf_fpp[i], num_edges[i], BFS_PER_PAGE,
                               BITS_PER_BF, is_verify);
      if (super_partitions > 1) {
        if (is_verify) {
          if (fread(verify_edge_buf, F::split_size_bytes(), num_edges[i],
                    edge_fpp[i]) != num_edges[i])
            assert(0);
          assert(memcmp(verify_edge_buf, edge_bufs[i],
                        num_edges[i] * F::split_size_bytes()) == 0);
        } else {
          if (fwrite(edge_bufs[i], F::split_size_bytes(), num_edges[i],
                     edge_fpp[i]) != num_edges[i])
            assert(0);
        }
      }
      num_edges[i] = 0;
    }
  }

  free(verify_edge_buf);
  free(edge_buf);
  for (unsigned int i = 0; i < super_partitions; ++i) {
    free(edge_bufs[i]);
    free(bf_bufs[i]);
    free(trans_bf_bufs[i]);
  }
}

template <typename A, typename F>
unsigned long long scatter_gather<A, F>::file_size(FILE *fp,
                                                   unsigned int unit_size) {
  fseeko(fp, 0L, SEEK_END);
  unsigned long long f_size = ftello(fp);
  assert(f_size % unit_size == 0);
  fseeko(fp, 0L, SEEK_SET);
  return f_size / unit_size;
}

template <typename A, typename F>
unsigned long long scatter_gather<A, F>::sanity_check(
    FILE *edge_fp, std::vector<FILE *> edge_fpp, std::vector<FILE *> bf_fpp,
    std::vector<unsigned long long> &num_edges, unsigned long long *start_edge,
    int BITS_PER_BF, int BFS_PER_PAGE, int SIMD_LEN) {
  assert(edge_fpp.size() == bf_fpp.size());
  assert(edge_fpp.size() == num_edges.size());
  unsigned int bf_buf_size = SIMD_LEN * BITS_PER_BF / 8;
  unsigned int edges_per_buf = BFS_PER_PAGE * SIMD_LEN;
  unsigned long long num_edge = file_size(edge_fp, F::split_size_bytes());

  std::string bf_mode = vm["bf_mode"].as<std::string>();
  if (bf_fpp.size() == 1) {
    unsigned long long bf_size = file_size(bf_fpp[0], 1);
    unsigned long long filter_group =
        (num_edge % (BFS_PER_PAGE * SIMD_LEN) != 0)
            ? num_edge / (BFS_PER_PAGE * SIMD_LEN) + 1
            : num_edge / (BFS_PER_PAGE * SIMD_LEN);
    if (bf_mode == "use" || bf_mode == "verify") {
      //std::cout << "San: " << (filter_group * bf_buf_size) << " " << bf_size
      //          << std::endl;
      assert((filter_group * bf_buf_size) == bf_size);
      *start_edge = 0;
    } else if (bf_mode == "update") {
      assert((filter_group * bf_buf_size) >= bf_size);
      *start_edge = (bf_size / bf_buf_size - 1) * edges_per_buf;
    }
  } else {
    unsigned long long num_edge_sum = 0;
    for (unsigned int i = 0; i < bf_fpp.size(); ++i) {
      num_edges[i] = file_size(edge_fpp[i], F::split_size_bytes());
      unsigned long long one_bf_size = file_size(bf_fpp[i], 1);
      unsigned long long filter_group = (num_edges[i] % edges_per_buf != 0)
                                            ? num_edges[i] / edges_per_buf + 1
                                            : num_edges[i] / edges_per_buf;
      num_edge_sum += num_edges[i];
      if (bf_mode == "use" || bf_mode == "verify") {
        num_edges[i] = 0;
        std::cout << "San: " << (filter_group * bf_buf_size) << " "
                  << one_bf_size << std::endl;
        assert((filter_group * bf_buf_size) == one_bf_size);
      } else if (bf_mode == "update") {
        assert((filter_group * bf_buf_size) >= one_bf_size);
      }
    }
    if (bf_mode == "use" || bf_mode == "verify") {
      std::cout << "num_edge: " << num_edge << " num_edge_sum: " << num_edge_sum << std::endl;
      assert(num_edge == num_edge_sum);
      *start_edge = 0;
    } else if (bf_mode == "update") {
      assert(num_edge >= num_edge_sum);
      *start_edge = num_edge_sum;
    }
  }
  return num_edge;
}

template <typename A, typename F>
void scatter_gather<A, F>::partition_super_partition(
    x_lib::streamIO<scatter_gather> *graph_storage, std::string efile,
    int BITS_PER_BF) {
  unsigned long super_partitions =
      graph_storage->get_config()->super_partitions;
  int BFS_PER_PAGE = vm["bfs_per_page"].as<unsigned long>();
  std::string bf_mode = vm["bf_mode"].as<std::string>();
  int SIMD_LEN = 256;

  FILE *edge_fp = fopen(efile.c_str(), "rb");
  assert(edge_fp != NULL);
  std::vector<FILE *> edge_fpp(super_partitions, NULL),
      bf_fpp(super_partitions, NULL);
  std::string edge_fname, bf_fname;
  for (unsigned int i = 0; i < super_partitions; ++i) {
    edge_fname = "edges." + std::to_string(super_partitions + i);
    std::string file_mode("r+");
    if (bf_mode == "create") file_mode = "w+";
    if (super_partitions > 1) {
      edge_fpp[i] = fopen(edge_fname.c_str(), file_mode.c_str());
      assert(edge_fpp[i] != NULL);
    }
    if (super_partitions == 1)
      bf_fname = "bf";
    else
      bf_fname = "bf." + std::to_string(super_partitions + i);
    bf_fpp[i] = fopen(bf_fname.c_str(), file_mode.c_str());
    assert(bf_fpp[i] != NULL);
  }

  unsigned long long num_edge = 0, start_edge = 0;
  std::vector<unsigned long long> num_edges(super_partitions, 0);
  num_edge = sanity_check(edge_fp, edge_fpp, bf_fpp, num_edges, &start_edge,
                          BITS_PER_BF, BFS_PER_PAGE, SIMD_LEN);

  if (bf_mode != "use") {
    bool is_verify = false;
    if (bf_mode == "verify") is_verify = true;
    edge_distribution(super_partitions, edge_fp, edge_fpp, bf_fpp, num_edge,
                      num_edges, start_edge, BFS_PER_PAGE, BITS_PER_BF,
                      SIMD_LEN, is_verify);
  }

  for (unsigned int i = 0; i < super_partitions; ++i) {
    if (super_partitions > 1) fclose(edge_fpp[i]);
    fclose(bf_fpp[i]);
  }
  fclose(edge_fp);
}

template <typename A, typename F>
scatter_gather<A, F>::scatter_gather() {
  BOOST_LOG_TRIVIAL(info) << "SG-DRIVER-ORIGINAL";
  wall_clock.start();
  setup_time.start();
  heartbeat = (vm.count("heartbeat") > 0);
  measure_scatter_gather = (vm.count("measure_scatter_gather") > 0);
  unsigned long num_processors = vm["processors"].as<unsigned long>();
  per_processor_data **algo_pcpu_array =
      new per_processor_data *[num_processors];
  sg_pcpu::per_cpu_array = pcpu_array = new sg_pcpu *[num_processors];
  sg_pcpu::sync = new x_barrier(num_processors);
  sg_pcpu::do_algo_reduce = false;
  for (unsigned long i = 0; i < num_processors; i++) {
    pcpu_array[i] = new sg_pcpu();
    pcpu_array[i]->processor_id = i;
    pcpu_array[i]->update_bytes_in = 0;
    pcpu_array[i]->update_bytes_out = 0;
    pcpu_array[i]->edge_bytes_streamed = 0;
    pcpu_array[i]->partitions_processed = 0;
    algo_pcpu_array[i] = A::create_per_processor_data(i);
  }
  sg_pcpu::algo_pcpu_array = algo_pcpu_array;
  A::preprocessing();  // Note: ordering critical with the next statement
  graph_storage = new x_lib::streamIO<scatter_gather>();
  sg_pcpu::scatter_filter =
      new x_lib::filter(MAX(graph_storage->get_config()->cached_partitions,
                            graph_storage->get_config()->super_partitions),
                        num_processors);
  sg_pcpu::partition = graph_storage->get_config()->cached_partitions;
  sg_pcpu::BF_vertex =
      new vertex_t *[graph_storage->get_config()->cached_partitions];
  sg_pcpu::BF_vertex_cnt =
      new int[graph_storage->get_config()->cached_partitions];
  sg_pcpu::BF_vertex_nr = new int[graph_storage->get_config()->cached_partitions];
  unsigned int MAX_CHECK_V = vm["max_check_v"].as<unsigned long>();
  sg_pcpu::all_BF_vertex.clear();
  sg_pcpu::all_BF_vertex.resize(MAX_CHECK_V, (unsigned int)-1);
  sg_pcpu::BF_bitset.clear();
  int BFS_PER_PAGE = vm["bfs_per_page"].as<unsigned long>();
  BOOST_ASSERT_MSG(0 < BFS_PER_PAGE, "need number of BFs per page");
  sg_pcpu::BF_bitset.resize(
      (graph_storage->get_config()->edges / BFS_PER_PAGE + 1) / 8 + 512, 0);
  std::cout << "bitset size: "
            << ((graph_storage->get_config()->edges / BFS_PER_PAGE + 1) / 8 +
                512)
            << std::endl;
  sg_pcpu::ver_buf_chg.clear();
  sg_pcpu::ver_buf_chg.resize((graph_storage->get_config()->vertex_state_buffer_size / 512 + 1) / 8 + 1);
  std::cout << "vertex buffer change buffer size:" << sg_pcpu::ver_buf_chg.size() << std::endl;
  bitset_generate_table(sg_pcpu::count_table);
  sg_pcpu::bsp_phase = 0;
  gettimeofday(&prev, NULL);
  if (heartbeat) {
    BOOST_LOG_TRIVIAL(info)
        << clock::timestamp(&prev, &cur) << " Initial phase ";
  }

  vertex_stream = graph_storage->open_stream(
      "vertices", true, vm["vertices_disk"].as<unsigned long>(),
      graph_storage->get_config()->vertex_size);
  int BITS_PER_BF = vm["bits_per_BF"].as<unsigned long>();
  int SIMD_LEN = 256;
  BOOST_ASSERT_MSG(32 <= BITS_PER_BF && ((BITS_PER_BF % 8) == 0),
                   "At least 32 bit bloom filters.");
  std::string efile = pt.get<std::string>("graph.name");
  partition_super_partition(graph_storage, efile, BITS_PER_BF);
  if (graph_storage->get_config()->super_partitions == 1) {
    edge_stream = graph_storage->open_stream(
        (const char *)efile.c_str(), false,
        vm["input_disk"].as<unsigned long>(), F::split_size_bytes());
  } else {
    edge_stream = graph_storage->open_stream(
        "edges", false, vm["edges_disk"].as<unsigned long>(),
        F::split_size_bytes());
  }
  bloom_stream = graph_storage->open_stream(
      "bf", false, vm["input_disk"].as<unsigned long>(),
      BITS_PER_BF / 8 * SIMD_LEN);

  updates0_stream = graph_storage->open_stream(
      "updates0", true, vm["updates0_disk"].as<unsigned long>(),
      A::split_size_bytes());
  updates1_stream = graph_storage->open_stream(
      "updates1", true, vm["updates1_disk"].as<unsigned long>(),
      A::split_size_bytes());
  setup_time.stop();
}

template <typename F>
struct edge_type_wrapper {
  static unsigned long item_size() { return F::split_size_bytes(); }

  static unsigned long key(unsigned char *buffer) {
    return F::split_key(buffer, 0);
  }
};

template <typename A>
struct update_type_wrapper {
  static unsigned long item_size() { return A::split_size_bytes(); }

  static unsigned long key(unsigned char *buffer) {
    return A::split_key(buffer, 0);
  }
};

template <typename A, typename F>
void scatter_gather<A, F>::operator()() {
  const x_lib::configuration *config = graph_storage->get_config();
  // Edge split
  if (config->super_partitions > 1) {
    // assert(0);
    // sg_pcpu::current_step = phase_edge_split;
    // x_lib::do_stream< scatter_gather<A, F>, edge_type_wrapper<F>,
    // edge_type_wrapper<F> > (graph_storage, 0, init_stream, edge_stream,
    // NULL); graph_storage->close_stream(init_stream);
  }
  // Supersteps
  unsigned long PHASE = 0;
  bool global_stop = false;
  if (heartbeat) {
    BOOST_LOG_TRIVIAL(info) << clock::timestamp(&prev, &cur)
                            << " Completed phase " << sg_pcpu::bsp_phase;
  }
  while (true) {
    sg_pcpu::current_step = superphase_begin;
    x_lib::do_cpu<scatter_gather<A, F> >(graph_storage, ULONG_MAX);
    unsigned long updates_in_stream =
        (PHASE == 0 ? updates1_stream : updates0_stream);
    unsigned long updates_out_stream =
        (PHASE == 0 ? updates0_stream : updates1_stream);
    graph_storage->rewind_stream(edge_stream);
    for (unsigned long i = 0; i < graph_storage->get_config()->super_partitions;
         i++) {
      if (graph_storage->get_config()->super_partitions > 1) {
        if (sg_pcpu::bsp_phase > 0) {
          graph_storage->state_load(vertex_stream, i);
        }
        graph_storage->state_prepare(i);
      } else if (sg_pcpu::bsp_phase == 0) {
        graph_storage->state_prepare(0);
      }
      if (A::need_init(sg_pcpu::bsp_phase)) {
        if (measure_scatter_gather) {
          state_iter_cost.start();
        }
        x_lib::do_state_iter<scatter_gather<A, F> >(graph_storage, i);
        if (measure_scatter_gather) {
          state_iter_cost.stop();
        }
      }
      sg_pcpu::current_step = phase_gather;
      if (heartbeat) {
        BOOST_LOG_TRIVIAL(info) << clock::timestamp(&prev, &cur)
                                << " Gather phase " << sg_pcpu::bsp_phase;
      }
      if (measure_scatter_gather) {
        gather_cost.start();
      }
      x_lib::do_stream<scatter_gather<A, F>, update_type_wrapper<A>,
                       update_type_wrapper<A> >(
          graph_storage, i, updates_in_stream, ULONG_MAX, NULL);
      if (measure_scatter_gather) {
        gather_cost.stop();
      }
      graph_storage->reset_stream(updates_in_stream, i);
      sg_pcpu::current_step = phase_BF_checking_1;
      if (heartbeat) {
        BOOST_LOG_TRIVIAL(info) << clock::timestamp(&prev, &cur)
                                << " BF checking phase " << sg_pcpu::bsp_phase;
      }
      for (int j = 0; j < sg_pcpu::partition; ++j) {
        sg_pcpu::BF_vertex_nr[j] = 0;
      }
      x_lib::do_state_iter<scatter_gather<A, F> >(graph_storage, i);
      sg_pcpu::all_BF_cnt = 0;
      for (int j = 0; j < sg_pcpu::partition; ++j) {
        sg_pcpu::all_BF_cnt += sg_pcpu::BF_vertex_nr[j];
      }
      printf("(%lu) : %u\n", sg_pcpu::bsp_phase, sg_pcpu::all_BF_cnt);
      bitset_clear(sg_pcpu::BF_bitset);
      unsigned int MAX_CHECK_V = vm["max_check_v"].as<unsigned long>() / graph_storage->get_config()->super_partitions;
      if (MAX_CHECK_V < 1) MAX_CHECK_V = 1;
      BOOST_ASSERT_MSG(0 < MAX_CHECK_V && MAX_CHECK_V < 100000000,
                       "Cannot exceed the maximum capability of the FPGA.");
      if (A::min_super_phases() - 1 <= sg_pcpu::bsp_phase &&
          sg_pcpu::all_BF_cnt < MAX_CHECK_V && MAX_CHECK_V != 1) {
        sg_pcpu::current_step = phase_BF_checking_2;
        if (heartbeat) {
          BOOST_LOG_TRIVIAL(info) << clock::timestamp(&prev, &cur)
                                << " BF checking2 phase " << sg_pcpu::bsp_phase;
        }
        for (int j = 0; j < sg_pcpu::partition; ++j) {
          sg_pcpu::BF_vertex_cnt[j] = 0;
          sg_pcpu::BF_vertex[j] = new vertex_t[sg_pcpu::BF_vertex_nr[j] + 1];
        }
        x_lib::do_state_iter<scatter_gather<A, F> >(graph_storage, i);
        sg_pcpu::all_BF_cnt = 0;
        for (int j = 0; j < sg_pcpu::partition; ++j) {
          for (int k = 0; k < sg_pcpu::BF_vertex_cnt[j]; ++k) {
            sg_pcpu::all_BF_vertex[sg_pcpu::all_BF_cnt + k] =
              sg_pcpu::BF_vertex[j][k];
          }
          sg_pcpu::all_BF_cnt += sg_pcpu::BF_vertex_cnt[j];
        }
        for (int j = 0; j < sg_pcpu::partition; ++j) {
          delete sg_pcpu::BF_vertex[j];
        }
        printf("(%lu) : %u\n", sg_pcpu::bsp_phase, sg_pcpu::all_BF_cnt);
        int CHIP_V_ONCE = vm["chip_v_once"].as<unsigned long>();
        BOOST_ASSERT_MSG(
            0 < CHIP_V_ONCE,
            "need to assign the minimum number of vertexes per round");
        if ((sg_pcpu::all_BF_cnt % CHIP_V_ONCE) != 0) {
          unsigned new_all_BF_cnt =
            (sg_pcpu::all_BF_cnt / CHIP_V_ONCE + 1) * CHIP_V_ONCE;
          for (unsigned int i = sg_pcpu::all_BF_cnt; i < new_all_BF_cnt; ++i) {
            sg_pcpu::all_BF_vertex[i] =
                sg_pcpu::all_BF_vertex[sg_pcpu::all_BF_cnt - 1];
          }
          sg_pcpu::all_BF_cnt = new_all_BF_cnt;
        }
        x_lib::do_meta_stream<scatter_gather<A, F>, update_type_wrapper<A>,
                              update_type_wrapper<A> >(
            graph_storage, i, bloom_stream, ULONG_MAX, NULL);
        graph_storage->restart_stream(bloom_stream, i);
        sg_pcpu::BF_bitset[0] |= 1;
        printf("%u/%u\n",
               bitset_count(sg_pcpu::BF_bitset, sg_pcpu::count_table),
               bitset_size(sg_pcpu::BF_bitset));
      }
      sg_pcpu::current_step = phase_scatter;
      if (heartbeat) {
        BOOST_LOG_TRIVIAL(info) << clock::timestamp(&prev, &cur)
                                << " Scatter phase " << sg_pcpu::bsp_phase;
      }
      x_lib::do_cpu<scatter_gather<A, F> >(graph_storage, i);
      if (measure_scatter_gather) {
        scatter_cost.start();
      }
      if (A::min_super_phases() - 1 <= sg_pcpu::bsp_phase &&
          (sg_pcpu::all_BF_cnt < MAX_CHECK_V) && (MAX_CHECK_V != 1)) {
        graph_storage->set_bitset_stream(edge_stream, &sg_pcpu::BF_bitset);
      }
      x_lib::do_stream<scatter_gather<A, F>, edge_type_wrapper<F>,
                       update_type_wrapper<A> >(graph_storage, i, edge_stream,
                                                updates_out_stream,
                                                sg_pcpu::scatter_filter);
      graph_storage->clear_bitset_stream(edge_stream);
      if (measure_scatter_gather) {
        scatter_cost.stop();
      }
      if (graph_storage->get_config()->super_partitions > 1){
        std::fill(sg_pcpu::ver_buf_chg.begin(), sg_pcpu::ver_buf_chg.end(), 0);
	sg_pcpu::ver_buf_chg[0] |= 1;
        x_lib::do_state_iter<scatter_gather<A, F> >(graph_storage, i);
      }
      sg_pcpu::current_step = phase_post_scatter;
      if (heartbeat) {
        BOOST_LOG_TRIVIAL(info) << clock::timestamp(&prev, &cur)
                                << " Post scatter phase " << sg_pcpu::bsp_phase;
      }
      if (i == (graph_storage->get_config()->super_partitions - 1)) {
        sg_pcpu::do_algo_reduce = true;
      }
      global_stop = x_lib::do_cpu<scatter_gather<A, F> >(graph_storage, i);
      sg_pcpu::do_algo_reduce = false;
      if (graph_storage->get_config()->super_partitions > 1) {
        graph_storage->set_bitset_stream(vertex_stream, &sg_pcpu::ver_buf_chg); 
        graph_storage->state_store(vertex_stream, i);
	graph_storage->clear_bitset_stream(vertex_stream);
      }
    }
    graph_storage->rewind_stream(updates_out_stream);
    if (graph_storage->get_config()->super_partitions > 1) {
      graph_storage->rewind_stream(vertex_stream);
    }
    unsigned long no_voter;
    PHASE = 1 - PHASE;
    sg_pcpu::bsp_phase++;
    if (heartbeat) {
      BOOST_LOG_TRIVIAL(info) << clock::timestamp(&prev, &cur)
                              << " Completed phase " << sg_pcpu::bsp_phase;
    }
    if (sg_pcpu::bsp_phase > A::min_super_phases()) {
      if (global_stop) {
        break;
      }
      for (no_voter = 0; no_voter < graph_storage->get_config()->processors;
           no_voter++) {
        if (!pcpu_array[no_voter]->i_vote_to_stop) {
          break;
        }
      }
      if ((no_voter == graph_storage->get_config()->processors)) {
        break;
      }
    }
  }
  if (graph_storage->get_config()->super_partitions == 1) {
    graph_storage->state_store(vertex_stream, 0);
  }
  A::postprocessing();
  sg_pcpu::current_step = phase_terminate;
  x_lib::do_cpu<scatter_gather<A, F> >(graph_storage, ULONG_MAX);
  setup_time.start();
  graph_storage->terminate();
  setup_time.stop();
  wall_clock.stop();
  BOOST_LOG_TRIVIAL(info) << "CORE::PHASES " << sg_pcpu::bsp_phase;
  setup_time.print("CORE::TIME::SETUP");
  if (measure_scatter_gather) {
    state_iter_cost.print("CORE::TIME::STATE_ITER");
    gather_cost.print("CORE::TIME::GATHER");
    scatter_cost.print("CORE::TIME::SCATTER");
  }
  sg_pcpu::pc_clock.print("TIME_IN_PC_FN");
  wall_clock.print("CORE::TIME::WALL");
  if (vm.count("filename")) {
    setup_time.writeToFile(vm["filename"].as<std::string>().c_str(), false);
    wall_clock.writeToFile(vm["filename"].as<std::string>().c_str(), true);
  }
}

template <typename A, typename F>
void scatter_gather<A, F>::partition_pre_callback(unsigned long superp,
                                                  unsigned long partition,
                                                  per_processor_data *pcpu) {
  sg_pcpu *pcpu_actual = static_cast<sg_pcpu *>(pcpu);
  if (pcpu_actual->current_step == phase_gather) {
    pcpu_actual->activate_partition_for_scatter = false;
  }
}

template <typename A, typename F>
void scatter_gather<A, F>::partition_callback(
    x_lib::stream_callback_state *callback) {
  sg_pcpu *pcpu = static_cast<sg_pcpu *>(callback->cpu_state);
  if (pcpu->processor_id == 0) {
    sg_pcpu::pc_clock.start();
  }
  switch (sg_pcpu::current_step) {
    case phase_edge_split: {
      unsigned long bytes_to_copy =
          (callback->bytes_in < callback->bytes_out_max)
              ? callback->bytes_in
              : callback->bytes_out_max;
      callback->bytes_in -= bytes_to_copy;
      memcpy(callback->bufout, callback->bufin, bytes_to_copy);
      callback->bufin += bytes_to_copy;
      callback->bytes_out = bytes_to_copy;
      break;
    }
    case phase_gather: {
      pcpu->update_bytes_in += callback->bytes_in;
      while (callback->bytes_in) {
        bool activate = A::apply_one_update(
            callback->state, callback->bufin,
            sg_pcpu::algo_pcpu_array[pcpu->processor_id], pcpu->bsp_phase);
        callback->bufin += A::split_size_bytes();
        callback->bytes_in -= A::split_size_bytes();
        pcpu->activate_partition_for_scatter |= activate;
        pcpu->i_vote_to_stop = pcpu->i_vote_to_stop && !activate;
      }
      break;
    }
    case phase_BF_checking_2: {
      int BITS_PER_BF = vm["bits_per_BF"].as<unsigned long>();
      int BYTES_PER_BF = BITS_PER_BF / 8;
      BOOST_ASSERT_MSG(callback->BF_offset % BYTES_PER_BF == 0,
                       "Bloom filter offset should be at a unit of 512 bits.");
      BOOST_ASSERT_MSG(callback->bytes_in % BYTES_PER_BF == 0,
                       "Bloom filter should be at a unit of 512 bits.");

      if (vm["kernel"].as<std::string>() == "CPU") {
        BOOST_ASSERT_MSG(
            (callback->bytes_in % (BYTES_PER_BF * 256)) == 0,
            "Bloom filter group should be at a unit of 256 * BYTES_PER_BF");
        unsigned int filter_groups = callback->bytes_in / (BYTES_PER_BF * 256);
        unsigned long long two_bf_mask =
            ((BITS_PER_BF - 1) << 16) + (BITS_PER_BF - 1);
        BOOST_ASSERT_MSG((callback->BF_offset % (BYTES_PER_BF * 8)) == 0,
                         "Outputs are in a unit of byte");
        unsigned char *base_out =
            sg_pcpu::BF_bitset.data() + callback->BF_offset / BYTES_PER_BF / 8;
        // printf("bitset: %x base_out: %x\n", sg_pcpu::BF_bitset.data(),
        // base_out);
        //__m256i one = _mm256_set1_epi8(0xFF);
        for (unsigned int i = 0; i < filter_groups; ++i) {
          unsigned char *base_addr = callback->bufin + i * (BYTES_PER_BF * 256);
          __m256i out = _mm256_setzero_si256();
          /*if(i == 0){
                  printf("hash: %u %u %u %u\n", _mm_crc32_u64(0,
          sg_pcpu::all_BF_vertex[0]) & (BITS_PER_BF - 1),
                          (_mm_crc32_u64(0, sg_pcpu::all_BF_vertex[0]) >> 16) &
          (BITS_PER_BF - 1), _mm_crc32_u64(0xFFFFFFFF,
          sg_pcpu::all_BF_vertex[0]) & (BITS_PER_BF - 1),
                          (_mm_crc32_u64(0xFFFFFFFF, sg_pcpu::all_BF_vertex[0])
          >> 16) & (BITS_PER_BF - 1)); for(int j = 0; j < 256; ++j)
          printf("%x(%d) ", base_addr[+ j << 5], j); printf("\n");
          }*/
          for (unsigned k = 0; k < sg_pcpu::all_BF_cnt; k += 4) {
            unsigned int *p_vertexes = &sg_pcpu::all_BF_vertex[k];
            for (unsigned int j = 0; j < 4; ++j) {
              unsigned int tmp_v = p_vertexes[j];  // 5 means log2(256 bits / 8)
              unsigned long long tmp_hash = _mm_crc32_u64(0, tmp_v);
              unsigned long long hash1 = (tmp_hash & two_bf_mask) ;
              unsigned long long hash2 =
                  (_mm_crc32_u64(tmp_hash, tmp_v) & two_bf_mask) ;
              __m256i rowA =
                  _mm256_load_si256((__m256i *)(base_addr + ((hash1 & 0xFFFF) << 5)));
              __m256i rowB =
                  _mm256_load_si256((__m256i *)(base_addr + ((hash1 >> 16) << 5)));
              __m256i andAB = _mm256_and_si256(rowA, rowB);
              __m256i rowC =
                  _mm256_load_si256((__m256i *)(base_addr + ((hash2 & 0xFFFF) << 5)));
              __m256i rowD =
                  _mm256_load_si256((__m256i *)(base_addr + ((hash2 >> 16) << 5)));
              __m256i andCD = _mm256_and_si256(rowC, rowD);
              __m256i andABCD = _mm256_and_si256(andAB, andCD);
              out = _mm256_or_si256(out, andABCD);
            }
            /*if((k & 0xFFC) == 0xFFC){
                __m256i pcmp = _mm256_cmpeq_epi32(out, one);
                if (_mm256_movemask_epi8(pcmp) == 0xFFFFFFFF) break;
            }*/
          }
          _mm256_store_si256((__m256i *)(base_out + (i << 5)), out);
          /*if(i == 0){
                  for(int j = 0; j < 32; ++j)
                          printf("%x ", base_out[(i << 5) + j]);
                  printf("\n");
          }*/
        }
      } else if (vm["kernel"].as<std::string>() == "CPUF") {
        unsigned int hash_seed[4] = {100, 200, 300, 400};
        unsigned int tmp[4] = {0, 0, 0, 0};
        for (unsigned int i = 0; i < sg_pcpu::all_BF_cnt; ++i) {
          vertex_t tmp_v = sg_pcpu::all_BF_vertex[i];
          MurmurHash3_x86_32(&tmp_v, hash_seed[0], &tmp[0]);
          unsigned int hashA = tmp[0] & (BITS_PER_BF - 1);
          MurmurHash3_x86_32(&tmp_v, hash_seed[1], &tmp[1]);
          unsigned int hashB = tmp[1] & (BITS_PER_BF - 1);
          MurmurHash3_x86_32(&tmp_v, hash_seed[2], &tmp[2]);
          unsigned int hashC = tmp[2] & (BITS_PER_BF - 1);
          MurmurHash3_x86_32(&tmp_v, hash_seed[3], &tmp[3]);
          unsigned int hashD = tmp[3] & (BITS_PER_BF - 1);

          for (unsigned int k = 0; k < callback->bytes_in / BYTES_PER_BF; ++k) {
            unsigned char *bufin = &callback->bufin[k * BYTES_PER_BF];
            unsigned int result =
                ((bufin[hashA / 8] & (1 << (hashA % 8))) != 0) &
                ((bufin[hashB / 8] & (1 << (hashB % 8))) != 0) &
                ((bufin[hashC / 8] & (1 << (hashC % 8))) != 0) &
                ((bufin[hashD / 8] & (1 << (hashD % 8))) != 0);
            unsigned int bit_index = callback->BF_offset / BYTES_PER_BF + k;
            sg_pcpu::BF_bitset[bit_index / 8] |= (result << (bit_index % 8));
          }
        }
      } else {
        BOOST_ASSERT_MSG(0, "unsupport kernel type!");
      }
      // callback->BF_offset += callback->bytes_in;
      callback->bytes_in = 0;
      break;
    }
    case phase_scatter: {
      unsigned long tmp = callback->bytes_in;
      unsigned char *bufout = callback->bufout;
      while (callback->bytes_in) {
        if ((callback->bytes_out + A::split_size_bytes()) >
            callback->bytes_out_max) {
          break;
        }
        bool up = false;
	unsigned long  key = F::split_key(callback->bufin, 0);
	if (key != 0xFFFFFFFF){
            up = A::generate_update(
              callback->state, callback->bufin, bufout,
              sg_pcpu::algo_pcpu_array[pcpu->processor_id], sg_pcpu::bsp_phase);
        }
        callback->bufin += F::split_size_bytes();
        callback->bytes_in -= F::split_size_bytes();
        if (up) {
          callback->bytes_out += A::split_size_bytes();
          bufout += A::split_size_bytes();
        }
      }
      pcpu->update_bytes_out += callback->bytes_out;
      pcpu->edge_bytes_streamed += (tmp - callback->bytes_in);
      break;
    }
    default:
      BOOST_LOG_TRIVIAL(fatal) << "Unknown operation in stream callback !";
      exit(-1);
  }
  if (pcpu->processor_id == 0) {
    sg_pcpu::pc_clock.stop();
  }
}

template <typename A, typename F>
void scatter_gather<A, F>::partition_post_callback(unsigned long superp,
                                                   unsigned long partition,
                                                   per_processor_data *pcpu) {
  sg_pcpu *pcpu_actual = static_cast<sg_pcpu *>(pcpu);
  if (pcpu_actual->current_step == phase_gather) {
    if (pcpu_actual->activate_partition_for_scatter) {
      sg_pcpu::scatter_filter->q(partition);
    }
    pcpu_actual->partitions_processed++;
  }
}
}  // namespace algorithm

#endif
