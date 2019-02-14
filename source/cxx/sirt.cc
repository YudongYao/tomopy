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
//   TOMOPY class header

#include "common.hh"
#include "utils.hh"
#include "utils_cuda.hh"

BEGIN_EXTERN_C
#include "sirt.h"
#include "utils.h"
#include "utils_openacc.h"
#include "utils_openmp.h"
END_EXTERN_C

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <memory>
#include <numeric>

//======================================================================================//

int
cxx_sirt(const float* data, int dy, int dt, int dx, const float* center,
         const float* theta, float* recon, int ngridx, int ngridy, int num_iter)
{
    // check to see if the C implementation is requested
    bool use_c_algorithm = GetEnv<bool>("TOMOPY_USE_C_SIRT", false);
    use_c_algorithm      = GetEnv<bool>("TOMOPY_USE_C_ALGORITHMS", use_c_algorithm);
    // if C implementation is requested, return non-zero (failure)
    if(use_c_algorithm)
        return scast<int>(false);

    auto tid = GetThisThreadID();
    ConsumeParameters(tid);
    static std::atomic<int> active;
    int                     count = active++;

    START_TIMER(cxx_timer);
    TIMEMORY_AUTO_TIMER("");

    printf("[%lu]> %s : nitr = %i, dy = %i, dt = %i, dx = %i, nx = %i, ny = %i\n",
           GetThisThreadID(), __FUNCTION__, num_iter, dy, dt, dx, ngridx, ngridy);

    {
        TIMEMORY_AUTO_TIMER("");
        run_algorithm(sirt_cpu, sirt_cuda, sirt_openacc, sirt_openmp, data, dy, dt, dx,
                      center, theta, recon, ngridx, ngridy, num_iter);
    }

    auto tcount = GetEnv("TOMOPY_PYTHON_THREADS", HW_CONCURRENCY);
    auto remain = --active;
    REPORT_TIMER(cxx_timer, __FUNCTION__, count, tcount);
    if(remain == 0)
    {
        std::stringstream ss;
        PrintEnv(ss);
        printf("[%lu] Reporting environment...\n\n%s\n", GetThisThreadID(),
               ss.str().c_str());
    }
    else
    {
        printf("[%lu] Threads remaining: %i...\n", GetThisThreadID(), remain);
    }

    return scast<int>(true);
}

//======================================================================================//

void
compute_projection(int dy, int dt, int dx, int nx, int ny, const float* theta, int s,
                   int p, float* m_recon, float* m_simdata, const float* m_data,
                   float* update)
{
    static Mutex                                         _mutex;
    static thread_local std::unique_ptr<cpu_rotate_data> _cache =
        std::unique_ptr<cpu_rotate_data>(new cpu_rotate_data(GetThisThreadID(), dy, dt,
                                                             dx, nx, ny, m_data, m_recon,
                                                             m_simdata));

    // needed for recon to output at proper orientation
    float theta_p = fmodf(theta[p], pi) + halfpi;
    // these structures are cached and re-used
    const float* data    = m_data + s * dt * dx;
    float*       simdata = m_simdata + s * dt * dx;
    float*       recon   = m_recon + s * nx * ny;

    // reset intermediate data
    _cache->reset();

    auto& use_rot   = _cache->use_rot();
    auto& use_tmp   = _cache->use_tmp();
    auto& recon_rot = _cache->rot();
    auto& recon_tmp = _cache->tmp();

    typedef cpu_rotate_data::int_type int_type;

    // Forward-Rotate object
    cxx_rotate_ip<float>(recon_rot, recon, -theta_p, nx, ny);
    cxx_rotate_ip<int_type>(use_rot, use_tmp.data(), -theta_p, nx, ny, CPU_NN);

    for(int d = 0; d < dx; d++)
    {
        int   pix_offset = d * nx;  // pixel offset
        int   idx_data   = d + p * dx;
        float _sum       = 0.0f;
        int   _fnx       = 0;
        // instead of including all the offsets later in the index lookup, offset the
        // pointer itself. This should make it easier for compiler to apply SIMD
        const float* _data      = data + idx_data;
        float*       _simdata   = simdata + idx_data;
        float*       _recon_rot = recon_rot.data() + pix_offset;
        auto*        _use_rot   = use_rot.data() + pix_offset;

        // Calculate simulated data by summing up along x-axis
        for(int n = 0; n < nx; ++n)
            _sum += _recon_rot[n];

        *_simdata += _sum;

        // Calculte the relevant pixels after rotation
        for(int n = 0; n < nx; ++n)
            _fnx += (_use_rot[n] != 0) ? 1 : 0;

        if(_fnx != 0)
        {
            // Make update by backprojecting error along x-axis
            float upd = (*_data - *_simdata) / scast<float>(_fnx);

            // update rotation
            for(int n = 0; n < nx; ++n)
                _recon_rot[n] += upd;
        }
    }

    // Back-Rotate object
    cxx_rotate_ip<float>(recon_tmp, recon_rot.data(), theta_p, nx, ny);

    // update shared update array
    _mutex.lock();
    float* _update = update + s * nx * ny;
    for(uintmax_t i = 0; i < recon_tmp.size(); ++i)
        _update[i] += recon_tmp[i];
    _mutex.unlock();
}

//======================================================================================//

void
sirt_cpu(const float* data, int dy, int dt, int dx, const float* /*center*/,
         const float* theta, float* recon, int ngridx, int ngridy, int num_iter)
{
    printf("[%lu]> %s : nitr = %i, dy = %i, dt = %i, dx = %i, nx = %i, ny = %i\n",
           GetThisThreadID(), __FUNCTION__, num_iter, dy, dt, dx, ngridx, ngridy);

#if defined(TOMOPY_USE_PTL)
    decltype(HW_CONCURRENCY) min_threads = 1;
    auto                     pythreads = GetEnv("TOMOPY_PYTHON_THREADS", HW_CONCURRENCY);
    auto                     nthreads =
        std::max(GetEnv("TOMOPY_NUM_THREADS", HW_CONCURRENCY / pythreads), min_threads);
    TaskRunManager* run_man = cpu_run_manager();
    init_run_manager(run_man, nthreads);
    TaskManager* task_man = run_man->GetTaskManager();
    init_thread_data(run_man->GetThreadPool());
#endif

    TIMEMORY_AUTO_TIMER("");

    //----------------------------------------------------------------------------------//
    uintmax_t grid_size = scast<uintmax_t>(dy * ngridx * ngridy);
    uintmax_t proj_size = scast<uintmax_t>(dy * dt * dx);
    for(int i = 0; i < num_iter; i++)
    {
        START_TIMER(t_start);
        // reset the simulation data
        farray_t simdata(proj_size, 0.0f);
        farray_t update(grid_size, 0.0f);
#if defined(TOMOPY_USE_PTL)
        TaskGroup<void> tg;
        // For each slice and projection angle
        for(int p = 0; p < dt; ++p)
            for(int s = 0; s < dy; s++)
                task_man->exec(tg, compute_projection, dy, dt, dx, ngridx, ngridy, theta,
                               s, p, recon, simdata.data(), data, update.data());
        tg.join();
#else
        // For each slice and projection angle
        for(int p = 0; p < dt; ++p)
            for(int s = 0; s < dy; s++)
                compute_projection(dy, dt, dx, ngridx, ngridy, theta, s, p, recon,
                                   simdata.data(), data, update.data());
#endif

        for(uintmax_t j = 0; j < grid_size; ++j)
            recon[j] += update[j] / scast<float>(dx);

        REPORT_TIMER(t_start, "iteration", i, num_iter);
    }

    printf("\n");
}

//======================================================================================//
#if !defined(TOMOPY_USE_CUDA)
void
sirt_cuda(const float* data, int dy, int dt, int dx, const float* center,
          const float* theta, float* recon, int ngridx, int ngridy, int num_iter)
{
    sirt_cpu(data, dy, dt, dx, center, theta, recon, ngridx, ngridy, num_iter);
}
#endif
//======================================================================================//

void
sirt_openacc(const float* data, int dy, int dt, int dx, const float*, const float* theta,
             float* recon, int ngridx, int ngridy, int num_iter)
{
    printf("[%lu]> %s : nitr = %i, dy = %i, dt = %i, dx = %i, nx = %i, ny = %i\n",
           GetThisThreadID(), __FUNCTION__, num_iter, dy, dt, dx, ngridx, ngridy);

    TIMEMORY_AUTO_TIMER("[openacc]");

    uintmax_t grid_size = scast<uintmax_t>(ngridx * ngridy);
    uintmax_t proj_size = scast<uintmax_t>(dy * dt * dx);
    for(int i = 0; i < num_iter; i++)
    {
        START_TIMER(t_start);
        farray_t simdata(proj_size, 0.0f);
        // For each slice
        for(int s = 0; s < dy; s++)
        {
            farray_t update(grid_size, 0.0f);
            farray_t recon_off(grid_size, 0.0f);

            int    slice_offset = s * ngridx * ngridy;
            float* _recon       = recon + slice_offset;

            // recon offset for the slice
            for(uintmax_t ii = 0; ii < recon_off.size(); ++ii)
                recon_off[ii] = _recon[ii];

            // For each projection angle
            for(int p = 0; p < dt; p++)
            {
                openacc_compute_projection(dt, dx, ngridx, ngridy, data, theta, s, p,
                                           simdata.data(), update.data(),
                                           recon_off.data());
            }

            for(uintmax_t ii = 0; ii < grid_size; ++ii)
                _recon[ii] += update[ii] / static_cast<float>(dx);
        }
        REPORT_TIMER(t_start, "iteration", i, num_iter);
    }
}

//======================================================================================//

void
sirt_openmp(const float* data, int dy, int dt, int dx, const float*, const float* theta,
            float* recon, int ngridx, int ngridy, int num_iter)
{
    printf("[%lu]> %s : nitr = %i, dy = %i, dt = %i, dx = %i, nx = %i, ny = %i\n",
           GetThisThreadID(), __FUNCTION__, num_iter, dy, dt, dx, ngridx, ngridy);

    TIMEMORY_AUTO_TIMER("[openmp]");

    uintmax_t grid_size = scast<uintmax_t>(ngridx * ngridy);
    uintmax_t proj_size = scast<uintmax_t>(dy * dt * dx);
    for(int i = 0; i < num_iter; i++)
    {
        START_TIMER(t_start);
        farray_t simdata(proj_size, 0.0f);
        // For each slice
        for(int s = 0; s < dy; s++)
        {
            farray_t update(grid_size, 0.0f);
            farray_t recon_off(grid_size, 0.0f);

            int    slice_offset = s * ngridx * ngridy;
            float* _recon       = recon + slice_offset;

            // recon offset for the slice
            for(uintmax_t ii = 0; ii < recon_off.size(); ++ii)
                recon_off[ii] = _recon[ii];

            // For each projection angle
            for(int p = 0; p < dt; p++)
            {
                openmp_compute_projection(dt, dx, ngridx, ngridy, data, theta, s, p,
                                          simdata.data(), update.data(),
                                          recon_off.data());
            }

            for(uintmax_t ii = 0; ii < grid_size; ++ii)
                _recon[ii] += update[ii] / static_cast<float>(dx);
        }
        REPORT_TIMER(t_start, "iteration", i, num_iter);
    }
}

//======================================================================================//