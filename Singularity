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


%environment
    . /usr/local/rivetenv.sh


%runscript
    Sherpa "$@"


%files
    HepMC-2.06.09.tar.gz
    rivet-bootstrap


%post
    echo Installing yum dependencies ...
    yum -y groupinstall "Development Tools"
    yum -y install \
        gsl-devel \
        make \
        python-devel \
        wget \
        zlib-devel \

    echo Bootstrapping Rivet, FastJet, YODA and HepMC
    mkdir -p /scratch/rivet
    cd /scratch/rivet
    mv /rivet-bootstrap .
    mv /HepMC-2.06.09.tar.gz .
    ls -lh
    tar -t HepMC-2.06.09.tar.gz
    chmod +x rivet-bootstrap
    INSTALL_PREFIX=/usr/local MAKE="make -j8" RIVET_CONFFLAGS="CXXFLAGS=-std=c++11" \
        ./rivet-bootstrap
    cd ..

    echo Installing LHAPDF
    wget https://lhapdf.hepforge.org/downloads/?f=LHAPDF-6.2.1.tar.gz -O- | tar xz
    cd LHAPDF-6.2.1
    ./configure --prefix=/usr/local
    make -j && make install
    cd ..

    echo Installing Sherpa
    wget https://sherpa.hepforge.org/downloads/?f=SHERPA-MC-2.2.6.tar.gz -O- | tar xz
    cd SHERPA-MC-2.2.6
    ./configure \
        --prefix=/usr/local \
        --enable-fastjet=/usr/local \
        --enable-hepmc2=/usr/local \
        --enable-lhapdf=/usr/local \
        --enable-rivet=/usr/local \
        --with-sqlite3=install \
        CXXFLAGS="-O3 --std=c++11"
    make -j && make install
    cd ..

    cd /
    rm -rf /scratch

    ldconfig
    yum clean all
