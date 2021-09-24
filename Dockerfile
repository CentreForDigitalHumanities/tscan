# pinned to this version of develop (known to work)
FROM proycon/lamachine@sha256:469c97a7344cbe6de8c316177b2614de6e0e73990e1deee0a3745e94bbf3b745

# fallback for old configuration location
COPY docker/*config.* /deployment/
COPY docker/deployment/ /deployment/
WORKDIR /deployment
RUN bash config.sh

WORKDIR /usr/local/src
COPY . /usr/local/src/tscan

WORKDIR /deployment
RUN bash add-alpino.sh
RUN bash build.sh
RUN bash apply-config.sh

# these static files take up most of the space (1.6 GB)
# linked through a volume bind instead
WORKDIR /usr/local/src/tscan
RUN rm -rf data
