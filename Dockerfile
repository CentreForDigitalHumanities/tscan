# pinned to this version of develop (known to work)
FROM proycon/lamachine@sha256:8eacbcba4cbd2b73de2148f1353f0661bbbd7db4742b90684cc0ac3449f1774a

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
