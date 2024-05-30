# A base container which is used for both the builder and the final container
# This way rebuilds are speed up because the common dependencies are re-used.
# The final container is also smaller because it doesn't contain build dependencies.
FROM ubuntu:22.04 AS base
ENV DEBIAN_FRONTEND=noninteractive
ENV PATH /Alpino/bin:/Alpino/Tokenization:/usr/local/go/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
ENV ALPINO_HOME /Alpino
ENV LANG en_US.UTF-8
ENV LANGUAGE en_US.UTF-8
ENV LC_ALL en_US.UTF-8
ENV LD_LIBRARY_PATH :/Alpino/util:/Alpino/Tokenization:/Alpino/fadd:/Alpino/TreebankTools/IndexedCorpus:/Alpino/create_bin:/Alpino/util:/Alpino/create_bin/extralibs:/Alpino/create_bin/extralibs/boost:/usr/lib:/usr/local/lib
ENV TCL_LIBRARY /Alpino/create_bin/extralibs/tcl8.5
ENV TK_LIBRARY /Alpino/create_bin/extralibs/tk8.5

# Add libraries to standard path
RUN ldconfig /Alpino/boost /Alpino/fadd /Alpino/unix /Alpino/TreebankTools/IndexedCorpus

RUN apt-get update && apt-get install -y locales gettext
RUN echo "en_US UTF-8\nen_US.UTF-8 UTF-8" > /etc/locale.gen && locale-gen

# T-Scan dependencies:
RUN apt-get update && apt-get install -y libicu70 \
        libxml2 \
        libgomp1 \
        libexttextcat-2.0-0 \
# Alpino dependencies:
        libxft2 libxss1
# Clam dependencies:
RUN apt-get update && apt-get install -y --no-install-recommends \
        runit \
        curl \
        ca-certificates \
        nginx \
        uwsgi \
        uwsgi-plugin-python3 \
        build-essential \
        python3-dev \
        python3-pip \
        python3-yaml \
        python3-lxml \
        python3-requests \
        pkg-config \
        libmariadb-dev

# Support authentication through MariaDB/MySQL
RUN pip install mysqlclient

# T-Scan webservice dependencies:
RUN apt-get update && apt-get install -y antiword \
        libmagic1 \
# MCS (compound splitter) dependencies:
        default-jre \
# Frog dependency:
        netbase \
# (found out the hard way after getting the following error message and being lost as to why)
# failure in getaddrinfo: Servname not supported for ai_socktype
# We need sudo to be able to restart projects using the right user
        sudo

FROM base AS builder
RUN apt-get update && apt-get install -y autoconf \
        autoconf-archive \
        automake \
        autotools-dev \
        bash \
        bzip2 \
        ca-certificates \
        ccache \
        checkinstall \
        clang-tools \
        cppcheck \
        curl \
        expect \
        ffmpeg \
        flac \
        g++ \
        git \
        lame \
        libbz2-dev \
        libexttextcat-dev \
        libicu-dev \
        libjpeg-dev \
        libmad0 \
        libsm6 \
        libsox-fmt-mp3 \
        libtar-dev \
        libtcl8.6 \
        libtk8.6 \
        libtool \
        libwww-perl \
        libxml2-dev \
        libxslt1-dev \
        libxslt1.1 \
        make \
        pkg-config \
        poppler-utils \
        pstotext \
        python3-dev \
        python3-minimal \
        sox \
        subversion \
        swig \
        tesseract-ocr \
        tk \
        unrtf \
        unzip \
        wget \
        zip \
        zlib1g-dev

COPY docker/deployment/ /deployment/
# this might contain pre-downloaded Alpino
COPY data/ /src/tscan/data
# Pre-built packages and dependencies
# Acutal project files are excluded using dockerignore
COPY docker/data/ /src/tscan/docker/data/

WORKDIR /deployment
# Prepare and install all the dependencies
RUN ./add-alpino.sh
# These will create .deb packages which can be re-used by the final container
# or during a rebuild
RUN ./prep-dep.sh ticcutils https://github.com/LanguageMachines/ticcutils ecb62eb116ffc3677626c53f012f34e00854e4c4
RUN ./prep-dep.sh libfolia https://github.com/LanguageMachines/libfolia 1a3f462f8f048df60889817325701130b6271e8a
RUN ./prep-dep.sh uctodata https://github.com/LanguageMachines/uctodata a8be6cf7512998f4c8963d1021f402b8b1290085
RUN ./prep-dep.sh ucto https://github.com/LanguageMachines/ucto d22029833b264a969351ead0ee3ab9b3f97db97d
RUN ./prep-dep.sh timbl https://github.com/LanguageMachines/timbl 61929ce6dc7d7077cb5eeceafef86de38eb40daa
RUN ./prep-dep.sh mbt https://github.com/LanguageMachines/mbt fd7cb7ebdd52bef2794f16f329569bedad3143e9
RUN ./prep-dep.sh mbtserver https://github.com/LanguageMachines/mbtserver a6f04f30f62965c8660ee92be99d4eb86fc4bf65
RUN ./prep-dep.sh frogdata https://github.com/LanguageMachines/frogdata 99de9597105c2304faeb797264231ba180fcdb20
RUN ./prep-dep.sh frog https://github.com/LanguageMachines/frog 991f6977cfd81b9a6538db0c6de3d26908ec16b7
RUN ./prep-dep.sh wopr https://github.com/LanguageMachines/wopr 6e3068047c0f9d3c66fcedb9c25e68907770ac9d

RUN ./build-compound-splitter.sh

# Have the copying and build as the very last item,
# this way only the source itself will have to rebuild during a change
COPY . /src/tscan
RUN ./build.sh

FROM base AS tscan

COPY --from=builder /src/*.deb /src/
COPY --from=builder /src/*.tar.gz /src/
COPY --from=builder /src/compound-splitter/ /src/compound-splitter/
# Build output (for caching)
COPY --from=builder /src/tscan/src/*.o /src/tscan/src/
COPY --from=builder /src/tscan/webservice/ /src/tscan/webservice/
COPY --from=builder /src/tscan/view/ /src/tscan/view/
COPY --from=builder /Alpino/ /Alpino/
COPY --from=builder /deployment/ /deployment/

WORKDIR /deployment
RUN ./install.sh
