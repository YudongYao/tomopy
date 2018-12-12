#
#   Find packages
#

include(FindPackageHandleStandardArgs)


################################################################################
#
#                               Threading
#
################################################################################

if(NOT WIN32)
    set(CMAKE_THREAD_PREFER_PTHREAD ON)
    set(THREADS_PREFER_PTHREAD_FLAG ON)
endif()

find_package(Threads)
if(Threads_FOUND)
    list(APPEND EXTERNAL_LIBRARIES Threads::Threads)
endif()


################################################################################
#
#                               TiMemory
#
################################################################################

if(TOMOPY_USE_TIMEMORY)
    find_package(TiMemory)

    if(TiMemory_FOUND)
        list(APPEND EXTERNAL_INCLUDE_DIRS ${TiMemory_INCLUDE_DIRS})
        list(APPEND EXTERNAL_LIBRARIES
            ${TiMemory_LIBRARIES} ${TiMemory_C_LIBRARIES})
        add_definitions(-DTOMOPY_USE_TIMEMORY)
    endif(TiMemory_FOUND)

endif(TOMOPY_USE_TIMEMORY)


################################################################################
#
#        Google PerfTools
#
################################################################################

if(TOMOPY_USE_GPERF)
    find_package(GPerfTools COMPONENTS profiler tcmalloc)

    if(GPerfTools_FOUND)
        list(APPEND EXTERNAL_INCLUDE_DIRS ${GPerfTools_INCLUDE_DIRS})
        list(APPEND EXTERNAL_LIBRARIES ${GPerfTools_LIBRARIES})
        add_definitions(-DTOMOPY_USE_GPERF)
    endif(GPerfTools_FOUND)

endif(TOMOPY_USE_GPERF)


################################################################################
#
#        OpenMP
#
################################################################################

if(TOMOPY_USE_OPENMP AND TOMOPY_USE_GPU)
    find_package(OpenMP)

    if(OpenMP_FOUND)
        # Add the OpenMP-specific compiler and linker flags
        foreach(LANG C CXX)
            set(_FLAGS "${OpenMP_${LANG}_FLAGS}")
            if(CMAKE_${LANG}_COMPILER_IS_PGI)
                string(REPLACE "-mp" "-mp${PGI_OPENMP_TYPE}" _FLAGS "${_FLAGS}")
            endif()
            add(${PROJECT_NAME}_${LANG}_FLAGS "${_FLAGS}")
        endforeach()
        add_definitions(-DTOMOPY_USE_OPENMP)
    endif(OpenMP_FOUND)

endif(TOMOPY_USE_OPENMP AND TOMOPY_USE_GPU)


################################################################################
#
#        OpenACC
#
################################################################################

if(TOMOPY_USE_OPENACC AND TOMOPY_USE_GPU)
    find_package(OpenACC)

    foreach(LANG C CXX)
        if(OpenACC_${LANG}_FOUND)
            # Add the OpenACC flags
            add(${PROJECT_NAME}_${LANG}_FLAGS "${OpenACC_${LANG}_FLAGS}")
            add_definitions(-DTOMOPY_USE_OPENACC)
        endif(OpenACC_${LANG}_FOUND)
    endforeach()

endif(TOMOPY_USE_OPENACC AND TOMOPY_USE_GPU)


################################################################################
#
#        TBB
#
################################################################################

if(TOMOPY_USE_TBB)
    find_package(TBB COMPONENTS malloc malloc_proxy)

    if(TBB_FOUND)
        list(APPEND EXTERNAL_INCLUDE_DIRS ${TBB_INCLUDE_DIRS})
        list(APPEND EXTERNAL_LIBRARIES ${TBB_LIBRARIES})
        add_definitions(-DTOMOPY_USE_TBB)
    endif(TBB_FOUND)

endif(TOMOPY_USE_TBB)


################################################################################
#
#        MKL
#
################################################################################

if(TOMOPY_USE_MKL)
    find_package(PythonInterp REQUIRED)

    # anaconda should have installed MKL under this prefix
    if(PYTHON_EXECUTABLE)
        get_filename_component(_MKL_PREFIX ${PYTHON_EXECUTABLE} DIRECTORY)
        get_filename_component(_MKL_PREFIX ${_MKL_PREFIX} DIRECTORY)
        list(APPEND CMAKE_PREFIX_PATH ${_MKL_PREFIX} ${_MKL_PREFIX}/lib ${_MKL_PREFIX}/include)
    endif()

    find_package(MKL)

    if(MKL_FOUND)
        list(APPEND EXTERNAL_INCLUDE_DIRS ${MKL_INCLUDE_DIRS})
        list(APPEND EXTERNAL_LIBRARIES ${MKL_LIBRARIES})
        add_definitions(-DTOMOPY_USE_MKL)
        add_definitions(-DUSE_MKL)
    endif(MKL_FOUND)

endif(TOMOPY_USE_MKL)


################################################################################
#
#        CUDA
#
################################################################################

if(TOMOPY_USE_CUDA AND TOMOPY_USE_GPU)
    find_package(CUDA)

    if(CUDA_FOUND)
        foreach(_DIR ${CUDA_INCLUDE_DIRS})
            include_directories(SYSTEM ${_DIR})
        endforeach(_DIR ${CUDA_INCLUDE_DIRS})
        list(APPEND EXTERNAL_INCLUDE_DIRS ${CUDA_INCLUDE_DIRS})

        find_library(CUDA_LIBRARY
            NAMES cuda
            PATHS /usr/local/cuda
            HINTS /usr/local/cuda
            PATH_SUFFIXES lib lib64)

        find_library(CUDART_LIBRARY
            NAMES cudart
            PATHS /usr/local/cuda
            HINTS /usr/local/cuda
            PATH_SUFFIXES lib lib64)

        find_library(CUDART_STATIC_LIBRARY
            NAMES cudart_static
            PATHS /usr/local/cuda
            HINTS /usr/local/cuda
            PATH_SUFFIXES lib lib64)

        if(TOMOPY_USE_NVTX)
            find_library(NVTX_LIBRARY
                NAMES nvToolsExt
                PATHS /usr/local/cuda
                HINTS /usr/local/cuda
                PATH_SUFFIXES lib lib64)
        else()
            unset(NVTX_LIBRARY CACHE)
        endif()

        find_library(RT_LIBRARY
            NAMES rt
            PATHS /usr /usr/local /opt/local
            HINTS /usr /usr/local /opt/local
            PATH_SUFFIXES lib lib64)

        find_library(DL_LIBRARY
            NAMES dl
            PATHS /usr /usr/local /opt/local
            HINTS /usr /usr/local /opt/local
            PATH_SUFFIXES lib lib64)

        foreach(NAME CUDA CUDART CUDART_STATIC NVTX RT DL)
            if(${NAME}_LIBRARY)
                list(APPEND EXTERNAL_CUDA_LIBRARIES ${${NAME}_LIBRARY})
            endif()
        endforeach()

        #set(CUDA_GENERATED_OUTPUT_DIR ${CMAKE_BINARY_DIR}/BuildProducts/bin)
        set(CUDA_SEPARABLE_COMPILATION OFF)

        set(CUDA_ARCH "sm_35" CACHE STRING "CUDA architecture flag")
        add_feature(CUDA_ARCH "CUDA architecture (e.g. sm_35)")
        add(CUDA_NVCC_FLAGS "-arch=${CUDA_ARCH}")
        add_definitions(-DTOMOPY_USE_CUDA)
        if(NVTX_LIBRARY)
            add_definitions(-DTOMOPY_USE_NVTX)
        endif()
    endif()

endif(TOMOPY_USE_CUDA AND TOMOPY_USE_GPU)


################################################################################
#
#        ITTNOTIFY (for VTune)
#
################################################################################
if(TOMOPY_USE_ITTNOTIFY)

    find_package(ittnotify)

    if(ittnotify_FOUND)
        list(APPEND EXTERNAL_INCLUDE_DIRS ${ITTNOTIFY_INCLUDE_DIRS})
        list(APPEND EXTERNAL_LIBRARIES ${ITTNOTIFY_LIBRARIES})
    else()
        message(WARNING "ittnotify not found. Set \"VTUNE_AMPLIFIER_201{7,8,9}_DIR\" or \"VTUNE_AMPLIFIER_XE_201{7,8,9}_DIR\" in environment")
    endif()

endif()


################################################################################
#
#        External variables
#
################################################################################

# including the directories
safe_remove_duplicates(EXTERNAL_INCLUDE_DIRS ${EXTERNAL_INCLUDE_DIRS})
safe_remove_duplicates(EXTERNAL_LIBRARIES ${EXTERNAL_LIBRARIES})
foreach(_DIR ${EXTERNAL_INCLUDE_DIRS})
    include_directories(SYSTEM ${_DIR})
endforeach(_DIR ${EXTERNAL_INCLUDE_DIRS})

# configure package files
configure_file(${PROJECT_SOURCE_DIR}/tomopy/allocator/__init__.py.in
    ${PROJECT_SOURCE_DIR}/tomopy/allocator/__init__.py
    @ONLY)

configure_file(${PROJECT_SOURCE_DIR}/tomopy/allocator/__init__.py.in
    ${PROJECT_BINARY_DIR}/tomopy/allocator/__init__.py
    @ONLY)