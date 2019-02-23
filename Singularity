# The following header has been based on instructions in
# https://singularity.lbl.gov/build-docker-module
Bootstrap: docker
From: centos:7


%help
A Singularity file to define an image for running Sherpa 2.2.6 including
support for FastJet, HepMC2, LHAPDF, Rivet, OpenLoops and BlackHat.


%labels
    Maintainer Enrico Bothmann
    Version v0.1


%environment
    . /usr/local/rivetenv.sh


%runscript
    Sherpa "$@"


%files
    rivet-bootstrap


%post
    echo Installing yum dependencies ...
    yum -y groupinstall "Development Tools"
    yum -y install \
        gsl-devel \
        make \
        openssl-devel \
        python-devel \
        wget \
        which \
        zlib-devel \

    echo Bootstrapping Rivet, FastJet, YODA and HepMC
    mkdir -p /scratch/rivet
    cd /scratch/rivet
    mv /rivet-bootstrap .
    chmod +x rivet-bootstrap
    INSTALL_PREFIX=/usr/local MAKE="make -j8" RIVET_CONFFLAGS="CXXFLAGS=-std=c++11" \
        ./rivet-bootstrap

    echo Installing LHAPDF
    cd /scratch
    wget https://lhapdf.hepforge.org/downloads/?f=LHAPDF-6.2.1.tar.gz -O- | tar xz
    cd LHAPDF-6.2.1
    ./configure --prefix=/usr/local
    make -j && make install

    echo Installing OpenLoops
    mkdir -p /opt
    cd /opt
    wget https://openloops.hepforge.org/downloads?f=OpenLoops-1.3.1.tar.gz -O- | tar xz
    cd OpenLoops-1.3.1/
    ./scons
    ./openloops libinstall ppll ppllj pplljj

    echo Installing BlackHat
    cd /
    wget https://www.theorie.physik.uni-goettingen.de/~bothmann/blackhat-r3095.tar.gz -O- | tar xz

    echo Installing Sherpa
    cd /scratch
    wget https://sherpa.hepforge.org/downloads/?f=SHERPA-MC-2.2.6.tar.gz -O- | tar xz
    cd SHERPA-MC-2.2.6
    ./configure \
        --prefix=/usr/local \
        --enable-analysis \
        --enable-blackhat=/opt/blackhat-r3095 \
        --enable-fastjet=/usr/local \
        --enable-hepmc2=/usr/local \
        --enable-lhapdf=/usr/local \
        --enable-openloops=/opt/OpenLoops-1.3.1 \
        --enable-rivet=/usr/local \
        --with-sqlite3=install \
        CXXFLAGS="-O3 --std=c++11"
    make -j && make install
    cd ..

    cd /
    rm -rf /scratch

    ldconfig
    yum clean all
