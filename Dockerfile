FROM proycon/lamachine@sha256:e2c8530455187acdcc6bb45f1f4a84bd40a5c92237c7ebb7683969fb838f27c4

COPY docker/ /deployment/
WORKDIR /deployment
RUN bash config.sh

# TODO: this is probably some bug or configuration issue in LaMachine?
RUN bash -c "cp /usr/local/src/LaMachine/host_vars/develop.yml /usr/local/src/LaMachine/host_vars/localhost.yml"

# TODO: from source
WORKDIR /usr/local/src
RUN git clone https://github.com/UUDigitalHumanitieslab/tscan.git
COPY data/ /usr/local/src/tscan/data

WORKDIR /deployment
RUN bash install-alpino.sh
RUN bash build.sh

# these static files take up most of the space (1.6 GB)
# linked through a volume bind instead
WORKDIR /usr/local/src/tscan
RUN rm -rf data
