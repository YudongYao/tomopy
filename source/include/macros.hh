//  Copyright (c) 2015, UChicago Argonne, LLC. All rights reserved.
//  Copyright 2015. UChicago Argonne, LLC. This software was produced
//  under U.S. Government contract DE-AC02-06CH11357 for Argonne National
//  Laboratory (ANL), which is operated by UChicago Argonne, LLC for the
//  U.S. Department of Energy. The U.S. Government has rights to use,
//  reproduce, and distribute this software.  NEITHER THE GOVERNMENT NOR
//  UChicago Argonne, LLC MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR
//  ASSUMES ANY LIABILITY FOR THE USE OF THIS SOFTWARE.  If software is
//  modified to produce derivative works, such modified software should
//  be clearly marked, so as not to confuse it with the version available
//  from ANL.
//  Additionally, redistribution and use in source and binary forms, with
//  or without modification, are permitted provided that the following
//  conditions are met:
//      * Redistributions of source code must retain the above copyright
//        notice, this list of conditions and the following disclaimer.
//      * Redistributions in binary form must reproduce the above copyright
//        notice, this list of conditions and the following disclaimer in
//        the documentation andwith the
//        distribution.
//      * Neither the name of UChicago Argonne, LLC, Argonne National
//        Laboratory, ANL, the U.S. Government, nor the names of its
//        contributors may be used to endorse or promote products derived
//        from this software without specific prior written permission.
//  THIS SOFTWARE IS PROVIDED BY UChicago Argonne, LLC AND CONTRIBUTORS
//  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
//  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
//  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL UChicago
//  Argonne, LLC OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
//  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
//  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
//  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
//  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
//  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
//  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
//  POSSIBILITY OF SUCH DAMAGE.
//  ---------------------------------------------------------------
//   TOMOPY header

#pragma once

extern "C"
{
#include "macros.h"
}

//======================================================================================//
//  C headers

#include <cassert>
#include <cinttypes>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

//======================================================================================//
//  C++ headers

#include <algorithm>
#include <atomic>
#include <chrono>
#include <complex>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <new>
#include <numeric>
#include <ostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#ifdef TOMOPY_USE_TIMEMORY
#    include <timemory/timemory.hpp>
#else
#    include "profiler.hh"
#endif

#include "PTL/AutoLock.hh"
#include "PTL/Types.hh"
#include "PTL/Utility.hh"

#if defined(TOMOPY_USE_PTL)
#    include "PTL/TBBTaskGroup.hh"
#    include "PTL/Task.hh"
#    include "PTL/TaskGroup.hh"
#    include "PTL/TaskManager.hh"
#    include "PTL/TaskRunManager.hh"
#    include "PTL/ThreadData.hh"
#    include "PTL/ThreadPool.hh"
#    include "PTL/Threading.hh"
#endif

//--------------------------------------------------------------------------------------//

#if defined(TOMOPY_USE_CUDA)
#    include <cooperative_groups.h>
#    include <cuda.h>
#    include <cuda_runtime_api.h>
#    include <npp.h>
#    include <nppi.h>
#    include <vector_types.h>
#else
#    if !defined(cudaStream_t)
#        define cudaStream_t int
#    endif
#endif

#if defined(TOMOPY_USE_OPENCV)
#    include <opencv2/core.hpp>
#    include <opencv2/imgproc.hpp>
#    include <opencv2/imgproc/imgproc.hpp>
#endif

#if defined(TOMOPY_USE_IPP)
#    include <ipp.h>
#    include <ippdefs.h>
#    include <ippi.h>
#endif

//======================================================================================//
// this function is used by a macro -- returns a unique identifier to the thread
inline uintmax_t
GetThisThreadID()
{
#if defined(TOMOPY_USE_PTL)
    return ThreadPool::GetThisThreadID();
#else
    static std::atomic<uintmax_t> tcounter;
    static thread_local auto      tid = tcounter++;
    return tid;
#endif
}

//======================================================================================//

#if !defined(scast)
#    define scast static_cast
#endif

//======================================================================================//

#if !defined(HW_CONCURRENCY)
#    define HW_CONCURRENCY std::thread::hardware_concurrency()
#endif

//======================================================================================//

#if !defined(_forward_args_t)
#    define _forward_args_t(_Args, _args) std::forward<_Args>(_args)...
#endif

//======================================================================================//

#if !defined(PRINT_HERE)
#    define PRINT_HERE(extra)                                                            \
        printf("[%lu]> %s@'%s':%i %s\n", GetThisThreadID(), __FUNCTION__, __FILE__,      \
               __LINE__, extra)
#endif

//======================================================================================//

#if !defined(START_TIMER)
#    define START_TIMER(var) auto var = std::chrono::system_clock::now()
#endif

//======================================================================================//

#if !defined(REPORT_TIMER)
#    define REPORT_TIMER(start_time, note, counter, total_count)                         \
        {                                                                                \
            auto                          end_time = std::chrono::system_clock::now();   \
            std::chrono::duration<double> elapsed_seconds = end_time - start_time;       \
            printf("[%li]> %-16s :: %3i of %3i... %5.2f seconds\n", GetThisThreadID(),   \
                   note, counter, total_count, elapsed_seconds.count());                 \
        }
#endif

//======================================================================================//

#if !defined(TOMOPY_USE_CUDA)
#    if !defined(__global__)
#        define __global__
#    endif
#    if !defined(__device__)
#        define __device__
#    endif
#endif

#if defined(__NVCC__) && defined(TOMOPY_USE_CUDA)

//--------------------------------------------------------------------------------------//

#    if !defined(CUDA_CHECK_CALL)
#        define CUDA_CHECK_CALL(err)                                                     \
            {                                                                            \
                if(cudaSuccess != err)                                                   \
                {                                                                        \
                    std::stringstream ss;                                                \
                    ss << "cudaCheckError() failed at " << __FUNCTION__ << "@'"          \
                       << __FILE__ << "':" << __LINE__ << " : "                          \
                       << cudaGetErrorString(err);                                       \
                    fprintf(stderr, "%s\n", ss.str().c_str());                           \
                    printf("%s\n", ss.str().c_str());                                    \
                    throw std::runtime_error(ss.str().c_str());                          \
                }                                                                        \
            }
#    endif

#endif  // NVCC and TOMOPY_USE_CUDA

//======================================================================================//