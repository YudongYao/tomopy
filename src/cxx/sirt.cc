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

#include "PTL/TaskManager.hh"
#include "PTL/TaskRunManager.hh"
#include "utils.hh"

BEGIN_EXTERN_C
#include "sirt.h"
#include "utils.h"
#include "utils_cuda.h"
#include "utils_openacc.h"
#include "utils_openmp.h"
END_EXTERN_C

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <numeric>

#define PRAGMA_SIMD _Pragma("omp simd")
#define PRAGMA_SIMD_REDUCTION(var) _Pragma("omp simd reducton(+ : var)")
#define HW_CONCURRENCY std::thread::hardware_concurrency()

//============================================================================//

int
cxx_sirt(const float* data, int dy, int dt, int dx, const float* center,
         const float* theta, float* recon, int ngridx, int ngridy, int num_iter)
{
    // check to see if the C implementation is requested
    bool use_c_algorithm = GetEnv<bool>("TOMOPY_USE_C_SIRT", false);
    // if C implementation is requested, return non-zero (failure)
    if(use_c_algorithm)
        return (int) false;

    auto tid = ThreadPool::GetThisThreadID();
    ConsumeParameters(tid);

#if defined(TOMOPY_USE_TIMEMORY)
    tim::timer t(__FUNCTION__);
    t.format().get()->width(10);
    t.start();
#endif

    TIMEMORY_AUTO_TIMER("");
    printf("\n\t%s [nitr = %i, dy = %i, dt = %i, dx = %i, nx = %i, ny = %i]\n\n",
           __FUNCTION__, num_iter, dy, dt, dx, ngridx, ngridy);

#if defined(TOMOPY_USE_GPU)
    // TODO: select based on memory
    bool use_cpu = GetEnv<bool>("TOMOPY_USE_CPU", false);
    if(use_cpu)
        sirt_cpu(data, dy, dt, dx, center, theta, recon, ngridx, ngridy, num_iter);
    else
        run_gpu_algorithm(sirt_cpu, sirt_cuda, sirt_openacc, sirt_openmp, data, dy, dt,
                          dx, center, theta, recon, ngridx, ngridy, num_iter);
#else
    sirt_cpu(data, dy, dt, dx, center, theta, recon, ngridx, ngridy, num_iter);
#endif

#if defined(TOMOPY_USE_TIMEMORY)
    AutoLock l(TypeMutex<decltype(std::cout)>());
    std::cout << "[" << tid << "]> " << t.stop_and_return() << std::endl;
#endif

    return (int) true;
}

//============================================================================//

struct cpu_thread_data
{
    int       m_id;
    int       m_dy;
    int       m_dt;
    int       m_dx;
    int       m_nx;
    int       m_ny;
    uintmax_t m_size;
    farray_t  m_rot;
    farray_t  m_tmp;
    farray_t  m_recon;
    farray_t  m_update;
    farray_t  m_simdata;
    float*    m_data;

    cpu_thread_data(int id, int dy, int dt, int dx, int nx, int ny)
    : m_id(id)
    , m_dy(dy)
    , m_dt(dt)
    , m_dx(dx)
    , m_nx(nx)
    , m_ny(ny)
    , m_size(m_nx * m_ny)
    , m_rot(farray_t(m_size, 0.0f))
    , m_tmp(farray_t(m_size, 0.0f))
    , m_recon(farray_t(m_size, 0.0f))
    , m_update(farray_t(m_size, 0.0f))
    , m_simdata(farray_t(m_dt * m_dx, 0.0f))
    , m_data(new float[m_dt * m_dx])
    {
    }

    ~cpu_thread_data() { delete[] m_data; }

    void initialize(const float* data, const float* recon, uintmax_t s)
    {
        uintmax_t offset = s * m_dt * m_dx;
        memcpy(m_data, data + offset, m_dt * m_dx * sizeof(float));
        memset(m_simdata.data(), 0, m_dt * m_dx * sizeof(float));
        offset = s * m_size;
        memset(m_update.data(), 0, m_size * sizeof(float));
        memcpy(m_recon.data(), recon + offset, m_size * sizeof(float));
    }

    void finalize(float* recon, uintmax_t s)
    {
        uintmax_t offset = s * m_size;
        float*    _recon = recon + offset;
        float     factor = 1.0f / static_cast<float>(m_dt);
        for(uintmax_t i = 0; i < m_size; ++i)
            _recon[i] += m_update[i] * factor;
    }

    // void reset_simdata() { m_simdata = farray_t(m_dy * m_dt * m_dx, 0.0f); }

    farray_t& simdata() { return m_simdata; }
    farray_t& update() { return m_update; }
    farray_t& recon() { return m_recon; }
    farray_t& rot() { return m_rot; }
    farray_t& tmp() { return m_tmp; }

    const farray_t& simdata() const { return m_simdata; }
    const farray_t& update() const { return m_update; }
    const farray_t& recon() const { return m_recon; }
    const farray_t& rot() const { return m_rot; }
    const farray_t& tmp() const { return m_tmp; }
    const float*    data() const { return m_data; }
};

//============================================================================//

void
compute_projection(int dt, int dx, int ngridx, int ngridy, const float* theta, int s,
                   int p, int nthreads, cpu_thread_data** _thread_data)
{
    auto             thread_number = ThreadPool::GetThisThreadID() % nthreads;
    cpu_thread_data* _cache        = _thread_data[thread_number];

    // needed for recon to output at proper orientation
    float        pi_offset = 0.5f * (float) M_PI;
    float        fngridx   = ngridx;
    float        theta_p   = fmodf(theta[p] + pi_offset, 2.0f * (float) M_PI);
    farray_t&    simdata   = _cache->simdata();
    farray_t&    recon     = _cache->recon();
    farray_t&    update    = _cache->update();
    const float* data      = _cache->data();
    farray_t&    recon_rot = _cache->rot();
    farray_t&    recon_tmp = _cache->tmp();

    cxx_rotate_ip(recon_rot, recon, -theta_p, ngridx, ngridy);

    for(int d = 0; d < dx; d++)
    {
        int          pix_offset = d * ngridx;  // pixel offset
        int          idx_data   = d + p * dx;
        const float* _data      = data + idx_data;
        float*       _simdata   = simdata.data() + idx_data;
        float*       _recon_rot = recon_rot.data() + pix_offset;
        float        _sim       = 0.0f;

        // Calculate simulated data by summing up along x-axis
#pragma omp simd reduction(+ : _sim)
        for(int n = 0; n < ngridx; ++n)
            _sim += _recon_rot[n];

        // update shared simdata array
        *_simdata += _sim;

        // Make update by backprojecting error along x-axis
        float upd = (*_data - *_simdata) / fngridx;
#pragma omp simd
        for(int n = 0; n < ngridx; n++)
            _recon_rot[n] += upd;
    }
    // Back-Rotate object
    cxx_rotate_ip(recon_tmp, recon_rot, theta_p, ngridx, ngridy);

    // update shared update array
#pragma omp simd
    for(uintmax_t i = 0; i < recon_tmp.size(); ++i)
        update[i] += recon_tmp[i];
}

//============================================================================//

void
sirt_cpu(const float* data, int dy, int dt, int dx, const float*, const float* theta,
         float* recon, int ngridx, int ngridy, int num_iter)
{
    printf("\n\t%s [nitr = %i, dy = %i, dt = %i, dx = %i, nx = %i, ny = %i]\n\n",
           __FUNCTION__, num_iter, dy, dt, dx, ngridx, ngridy);

    TIMEMORY_AUTO_TIMER("");

    int             nthreads = GetEnv("TOMOPY_NUM_THREADS", HW_CONCURRENCY);
    TaskRunManager* run_man  = cpu_run_manager();
    init_run_manager(run_man, nthreads);
    TaskManager* task_man = run_man->GetTaskManager();

    cpu_thread_data** _thread_data = new cpu_thread_data*[nthreads];
    for(int i = 0; i < nthreads; ++i)
        _thread_data[i] = new cpu_thread_data(i, dy, dt, dx, ngridx, ngridy);

    for(int i = 0; i < num_iter; i++)
    {
        // For each slice
        for(int s = 0; s < dy; s++)
        {
            for(int i = 0; i < nthreads; ++i)
                _thread_data[i]->initialize(data, recon, s);

            TaskGroup<void> tg;
            // For each projection angle
            for(int p = 0; p < dt; p++)
            {
                task_man->exec(tg, compute_projection, dt, dx, ngridx, ngridy, theta, s,
                               p, nthreads, _thread_data);
            }
            tg.join();

            for(int i = 0; i < nthreads; ++i)
                _thread_data[i]->finalize(recon, s);
        }
    }
}

//============================================================================//

#if !defined(TOMOPY_USE_CUDA)
void
sirt_cuda(const float* data, int dy, int dt, int dx, const float* center,
          const float* theta, float* recon, int ngridx, int ngridy, int num_iter)
{
    sirt_cpu(data, dy, dt, dx, center, theta, recon, ngridx, ngridy, num_iter);
}
#endif

//============================================================================//

void
sirt_openacc(const float* data, int dy, int dt, int dx, const float*, const float* theta,
             float* recon, int ngridx, int ngridy, int num_iter)
{
    printf("\n\t%s [nitr = %i, dy = %i, dt = %i, dx = %i, nx = %i, ny = %i]\n\n",
           __FUNCTION__, num_iter, dy, dt, dx, ngridx, ngridy);

    TIMEMORY_AUTO_TIMER("[openacc]");

    for(int i = 0; i < num_iter; i++)
    {
        farray_t simdata(dy * dt * dx, 0.0f);
        // For each slice
        for(int s = 0; s < dy; s++)
        {
            farray_t update(ngridx * ngridy, 0.0f);
            farray_t recon_off(ngridx * ngridy, 0.0f);

            int    slice_offset = s * ngridx * ngridy;
            float* _recon       = recon + slice_offset;

            // recon offset for the slice
            for(int ii = 0; ii < recon_off.size(); ++ii)
                recon_off[ii] = _recon[ii];

            // For each projection angle
            for(int p = 0; p < dt; p++)
            {
                openacc_compute_projection(dt, dx, ngridx, ngridy, data, theta, s, p,
                                           simdata.data(), update.data(),
                                           recon_off.data());
            }

            for(int ii = 0; ii < (ngridx * ngridy); ++ii)
                _recon[ii] += update[ii] / static_cast<float>(dt);
        }
    }
}

//============================================================================//

void
sirt_openmp(const float* data, int dy, int dt, int dx, const float*, const float* theta,
            float* recon, int ngridx, int ngridy, int num_iter)
{
    printf("\n\t%s [nitr = %i, dy = %i, dt = %i, dx = %i, nx = %i, ny = %i]\n\n",
           __FUNCTION__, num_iter, dy, dt, dx, ngridx, ngridy);

    TIMEMORY_AUTO_TIMER("[openmp]");

    for(int i = 0; i < num_iter; i++)
    {
        farray_t simdata(dy * dt * dx, 0.0f);
        // For each slice
        for(int s = 0; s < dy; s++)
        {
            farray_t update(ngridx * ngridy, 0.0f);
            farray_t recon_off(ngridx * ngridy, 0.0f);

            int    slice_offset = s * ngridx * ngridy;
            float* _recon       = recon + slice_offset;

            // recon offset for the slice
            for(int ii = 0; ii < recon_off.size(); ++ii)
                recon_off[ii] = _recon[ii];

            // For each projection angle
            for(int p = 0; p < dt; p++)
            {
                openmp_compute_projection(dt, dx, ngridx, ngridy, data, theta, s, p,
                                          simdata.data(), update.data(),
                                          recon_off.data());
            }

            for(int ii = 0; ii < (ngridx * ngridy); ++ii)
                _recon[ii] += update[ii] / static_cast<float>(dt);
        }
    }
}

//============================================================================//
