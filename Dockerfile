################################################################################
#   Build stage 1
################################################################################
ARG CUDA_VERSION=10.0
ARG UBUNTU_RELEASE=18
FROM nvidia/cuda:${CUDA_VERSION}-devel-ubuntu${UBUNTU_RELEASE}.04 as builder

USER root
ENV HOME /root
ENV LANG en_US.UTF-8
ENV LANGUAGE en_US
ENV LC_ALL C
ENV SHELL /bin/bash
ENV BASH_ENV /etc/bash.bashrc
ENV DEBIAN_FRONTEND noninteractive

ARG PYTHON_VERSION=3.6
ARG GCC_VERSION=7
ARG CLANG_VERSION=6
WORKDIR /work

#------------------------------------------------------------------------------#
#   copy files for build
#------------------------------------------------------------------------------#
COPY ./.docker/apt.sh /work/apt.sh
COPY ./.docker/conda.sh /work/conda.sh
COPY ./envs /work/envs


#------------------------------------------------------------------------------#
#   environment
#------------------------------------------------------------------------------#
ENV PYTHON_VERSION ${PYTHON_VERSION}


#------------------------------------------------------------------------------#
#   primary install
#------------------------------------------------------------------------------#
RUN ./apt.sh && \
    ./conda.sh


#------------------------------------------------------------------------------#
#   PGI compiler
#------------------------------------------------------------------------------#
COPY ./.docker/pgilinux-2018-1810-x86-64.tar.gz /work/pgi/
ENV PGI_ACCEPT_EULA accept
ENV PGI_SILENT true
RUN cd /work/pgi && \
    tar -xzf pgilinux-2018-1810-x86-64.tar.gz && \
    ./install


#------------------------------------------------------------------------------#
#   Path configuration
#------------------------------------------------------------------------------#
COPY ./.docker/config-path.sh /work/config-path.sh
RUN ./config-path.sh


#------------------------------------------------------------------------------#
#   Repos (OpenCV)
#------------------------------------------------------------------------------#
COPY ./.docker/repos.sh /work/repos.sh
RUN ./repos.sh


#------------------------------------------------------------------------------#
#   Cleanup
#------------------------------------------------------------------------------#
RUN rm -rf /root/* /work/*


#------------------------------------------------------------------------------#
#   copy files for runtime
#------------------------------------------------------------------------------#
COPY ./.docker/runtime-entrypoint.sh /tomopy-gpu-runtime-entrypoint.sh
COPY ./.docker/compute-dir-size.py /etc/compute-dir-size.py
COPY ./.docker/bash.bashrc /etc/bash.bashrc
COPY ./.docker/bashrc /root/.bashrc


################################################################################
#   Build stage 2 - compress to 1 layer
################################################################################
FROM scratch

ARG REQUIRE_CUDA_VERSION=9.2

COPY --from=builder / /

ENV HOME /root
ENV LANG en_US.UTF-8
ENV LANGUAGE en_US
ENV LC_ALL C
ENV SHELL /bin/bash
ENV BASH_ENV /etc/bash.bashrc
ENV DEBIAN_FRONTEND noninteractive

ENV CUDA_HOME "/usr/local/cuda"
ENV NVIDIA_REQUIRE_CUDA "cuda>=${REQUIRE_CUDA_VERSION}"
ENV NVIDIA_VISIBLE_DEVICES "all"
ENV NVIDIA_DRIVER_CAPABILITIES "compute,utility"

USER root
WORKDIR /home
SHELL [ "/bin/bash", "--login", "-c" ]
ENTRYPOINT [ "/tomopy-gpu-runtime-entrypoint.sh" ]
