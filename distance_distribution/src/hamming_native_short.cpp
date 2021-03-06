// Copyright 2016 Till Kolditz, Matthias Werner
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// 
// http://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "globals.h"
#include "hamming.h"
#include "algorithms.h"
#include <helper.h>

#include <omp.h>
#include <math.h>
#include <iostream>
#include <string.h>
using namespace std;
using namespace Hamming;

typedef uintll_t (*computeHamming_ft)(const uintll_t &);

template<typename T, 
         uintll_t BITCNT_DATA, 
         computeHamming_ft func, 
         bool WITH_1BIT,
         uintll_t BITCNT_HAMMING = (BITCNT_DATA == 8 ? 5 : ((BITCNT_DATA == 16) | (BITCNT_DATA == 24) ? 6 : 7)), 
         uintll_t BITCNT_MSG = BITCNT_DATA + BITCNT_HAMMING, 
         uintll_t CNT_COUNTS = BITCNT_MSG + 1ull, 
         uintll_t CNT_MESSAGES = 0x1ull << BITCNT_DATA,
         uintll_t CNT_WORDS = 1ull<<BITCNT_MSG                            
>
void countHammingUndetectableErrors(uint128_t* result_counts) 
{
  T counts[ CNT_COUNTS ] = {0};
#pragma omp parallel
  {
    T counts_local[ CNT_COUNTS ] = {0};
    T x,y;
    T distance;
/*
#pragma omp master
    {
      cout << "OpenMP using " << omp_get_num_threads() << " threads" << endl;
    }*/
#pragma omp for schedule(dynamic,1)
    for(T a=0; a<CNT_MESSAGES; ++a)
    {
      memset(counts_local, 0, CNT_COUNTS*sizeof(T));
      x = func(a);
      // valid codewords transitions
      for(T b=0; b<CNT_MESSAGES; ++b)
      {
        y = func(b);
        distance = computeDistance(x,y);
        ++counts_local[distance];
        if(WITH_1BIT)
        {
          for(T p=0;p<BITCNT_MSG;++p)
          {
            distance=computeDistance(x,y^(static_cast<T>(1)<<p));
            ++counts_local[distance];
          }
        }
      }
      // 4) Sum the counts
      for (uint_t i = 0; i < CNT_COUNTS; ++i) {
#pragma omp atomic
        counts[i] += counts_local[i];
      }      
    }
  }
  
  for(uint_t i=0;i<CNT_COUNTS; ++i){
    result_counts[i] = static_cast<uint128_t>(counts[i]);
    //printf("%u %12llu\n", i, counts[i]);
  }
}

void run_hamming_cpu_native_short(uintll_t n, int with_1bit, int file_output)
{
  Statistics stats;
  TimeStatistics results_cpu (&stats,CPU_WALL_TIME);
  int i_totaltime = results_cpu.add("Total Runtime", "s");
  results_cpu.setFactorAll(0.001);
  
  const uintll_t h = ( n==8 ? 5 : (n<32?6:7) );
  uint128_t* counts = new uint128_t[n+h+1];
  memset(counts, 0, (n+h+1)*sizeof(uint128_t));

  printf("Start Hamming Coding Algorithm - Native Short Approach (CPU)\n");
  results_cpu.start(i_totaltime);

  if(with_1bit)
  {
    if(n==8)
      countHammingUndetectableErrors<uintll_t, 8, computeHamming08,true>(counts);
    else if(n==16)
      countHammingUndetectableErrors<uintll_t, 16, computeHamming16,true>(counts);
    else
      countHammingUndetectableErrors<uintll_t, 24, computeHamming24,true>(counts);
  }else{
    if(n==8)
      countHammingUndetectableErrors<uintll_t, 8, computeHamming08,false>(counts);
    else if(n==16)
      countHammingUndetectableErrors<uintll_t, 16, computeHamming16,false>(counts);
    else
      countHammingUndetectableErrors<uintll_t, 24, computeHamming24,false>(counts);
  }

//    countHammingUndetectableErrors<uint64_t, 32>();

  results_cpu.stop(i_totaltime);
  process_result_hamming(counts, stats, n, h, file_output?"hamming_cpu_native_short":nullptr);
  delete[] counts;
}
