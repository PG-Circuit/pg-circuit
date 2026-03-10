# syntax=docker/dockerfile:1

ARG PG_MAJOR=17
FROM postgres:${PG_MAJOR}

ARG PG_MAJOR=17

RUN apt-get update \
	&& apt-get install -y --no-install-recommends \
		build-essential \
		postgresql-server-dev-${PG_MAJOR} \
		ca-certificates \
	&& rm -rf /var/lib/apt/lists/*

WORKDIR /build
COPY . /build

RUN make clean || true \
	&& make \
	&& make install

# Ensure the library is available early so hooks are active in every session
RUN echo "shared_preload_libraries = 'pg_circuit'" >> /usr/share/postgresql/postgresql.conf.sample
