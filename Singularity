# The following header has been based on instructions in
# https://singularity.lbl.gov/build-docker-module
Bootstrap: docker
From: centos:7


%help
A Singularity file to define an image for running Sherpa 2.2.6 including
support for FastJet, HepMC2, LHAPDF and Rivet.


%labels
    Maintainer Enrico Bothmann
    Version v0.1


%runscript
    Sherpa "$@"


%post
    echo Installing yum dependencies ...
    yum -y groupinstall "Development Tools"
    yum -y install \
        gsl-devel
        make \
        python-devel \
        wget \
        zlib-devel \

    echo Bootstrapping Rivet, FastJet, YODA and HepMC
    mkdir -p /scratch/rivet
    cd /scratch/rivet
    wget \
        https://phab.hepforge.org/source/rivetbootstraphg/browse/2.6.2/rivet-bootstrap?view=raw \
        -O rivet-bootstrap
    chmod +x rivet-bootstrap
    INSTALL_PREFIX=/usr/local MAKE="make -j8" RIVET_CONFFLAGS="CXXFLAGS=-std=c++11" \
        ./rivet-bootstrap
    . /usr/local/rivetenv.sh

    wget 
