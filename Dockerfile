# Build stage
FROM ubuntu:22.04 AS builder

# Install required build tools
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    python3 \
    python3-pip \
    python3-dev \
    python-is-python3 \
    doxygen \
    && rm -rf /var/lib/apt/lists/*

# Install mandatory dependencies
RUN apt-get update && apt-get install -y \
    libssl-dev \
    libnghttp2-dev \
    libsrtp2-dev \
    libsqlite3-dev \
    default-libmysqlclient-dev \
    && rm -rf /var/lib/apt/lists/*

# Install optional dependencies (enabled by default)
RUN apt-get update && apt-get install -y \
    libhiredis-dev \
    libprotobuf-dev \
    protobuf-compiler \
    libsnmp-dev \
    libxerces-c-dev \
    libjsoncpp-dev \
    libmbedtls-dev \
    && rm -rf /var/lib/apt/lists/*

# Install additional dependencies
RUN apt-get update && apt-get install -y \
    libtool \
    pkg-config \
    wget \
    zlib1g-dev \
    libpq-dev \
    libsoci-dev \
    libjansson-dev \
    libspeex-dev \
    libspeexdsp-dev \
    libopus-dev \
    libgsm1-dev \
    libxml2-dev \
    graphviz \
    yasm \
    && rm -rf /var/lib/apt/lists/*

# Ensure Python 3 is properly linked
RUN ln -sf /usr/bin/python3 /usr/bin/python

# Install Python dependencies
RUN pip3 install pystache six

# Set build arguments
ARG FLEXISIP_VERSION=2.4.1-extended-registration
ARG LINPHONESDK_VERSION=5.2.4
ARG njobs=4

# Copy the local repository
COPY . /root/flexisip

# Build Flexisip following README instructions
RUN cd /root/flexisip && \
    rm -rf ./build && mkdir -p ./build && \
    cmake -S . -B ./build \
          -DCMAKE_INSTALL_LIBDIR=lib \
          -DINTERNAL_MBEDTLS=OFF \
          -DENABLE_JWE_AUTH_PLUGIN=OFF \
          -DFLEXISIP_VERSION=${FLEXISIP_VERSION} \
          -DLINPHONESDK_VERSION=${LINPHONESDK_VERSION} \
          -DLINPHONESDK_DIR=/root/flexisip/linphone-sdk \
          > /tmp/cmake-output.log 2>&1 ; \
    rc=$? ; \
    if [ $rc -ne 0 ]; then \
      echo "CMAKE_CONFIGURE_FAILED" ; \
      grep -i "error\|fatal\|not found\|missing\|failed\|could not" /tmp/cmake-output.log | grep -v "^--" | tail -30 ; \
      exit 1 ; \
    fi ; \
    echo "CMAKE_CONFIGURE_OK"

RUN cd /root/flexisip && \
    make -C ./build -j${njobs}

RUN cd /root/flexisip && \
    make -C ./build install && \
    echo "=== Installed libraries ===" && \
    find /usr/local/lib -name "*.so*" | head -50 && \
    ls -la /usr/local/lib/libflexisip.so* || echo "libflexisip.so not in /usr/local/lib (check find output above)"

# Runtime stage
FROM ubuntu:22.04

# Install mandatory runtime dependencies
RUN apt-get update && apt-get install -y \
    libssl3 \
    libnghttp2-14 \
    libsrtp2-1 \
    libsqlite3-0 \
    default-mysql-client \
    wget \
    libmbedtls-dev \
    libjemalloc2 \
    iptables \
    && rm -rf /var/lib/apt/lists/* && \
    echo "Listing mbedtls libraries:" && \
    ls -la /usr/lib/x86_64-linux-gnu/libmbedtls* && \
    ln -s /usr/lib/x86_64-linux-gnu/libmbedtls.so /usr/lib/x86_64-linux-gnu/libmbedtls.so.13

# Install optional runtime dependencies (enabled by default)
RUN apt-get update && apt-get install -y \
    libhiredis0.14 \
    libprotobuf23 \
    libsnmp40 \
    libxerces-c3.2 \
    libjsoncpp25 \
    && rm -rf /var/lib/apt/lists/*

# Install additional runtime dependencies
RUN apt-get update && apt-get install -y \
    libpq5 \
    libsoci-core4.0 \
    libsoci-sqlite3-4.0 \
    libsoci-mysql4.0 \
    libsoci-postgresql4.0 \
    libjansson4 \
    libspeex1 \
    libspeexdsp1 \
    libopus0 \
    libgsm1 \
    libxml2 \
    python3 \
    python3-pip \
    && rm -rf /var/lib/apt/lists/*

# Install Firebase authentication dependencies for push notifications
RUN pip3 install --no-cache-dir \
    google-auth \
    google-auth-httplib2 \
    requests

# Workaround for dynamic linker needing libiconv.so at runtime
RUN ln -s /lib/x86_64-linux-gnu/libc.so.6 /usr/lib/x86_64-linux-gnu/libiconv.so && \
    ldconfig # Refresh linker cache

# Create necessary directories first
RUN mkdir -p /var/log/flexisip /home/cores /etc/flexisip /opt/belledonne-communications/bin /opt/belledonne-communications/lib /var/opt/belledonne-communications/log/flexisip /usr/local/share/flexisip

# Copy all shared libraries from builder stage
COPY --from=builder /usr/local/lib/*.so* /usr/local/lib/
COPY --from=builder /root/flexisip/build/bin/flexisip /opt/belledonne-communications/bin/
COPY --from=builder /usr/local/share/belr/grammars/ /usr/local/share/belr/grammars/
RUN ldconfig # Refresh linker cache AFTER copying library

# Copy files in order of dependency
COPY docker/backtrace.gdb /backtrace.gdb
COPY docker/flexisip-entrypoint.sh /flexisip-entrypoint.sh
RUN chmod +x /flexisip-entrypoint.sh && \
    sed -i 's/ulimit -c unlimited\*/ulimit -c 100000/' /flexisip-entrypoint.sh && \
    echo "=== Entrypoint script content ===" && \
    cat /flexisip-entrypoint.sh && \
    echo "=== End of entrypoint script ==="

# Copy Python scripts
COPY --from=builder /usr/local/share/flexisip/*.py /usr/local/share/flexisip/
RUN chmod +x /usr/local/share/flexisip/*.py

# Script to wait db before launch flexisip [Licence Apache2].
# Vendored under docker/ so image builds do not depend on a live GitHub download
# (CI failed 2026-08-12 fetching ufoscout/docker-compose-wait@2.2.1 with "No data received").
COPY docker/docker-compose-wait /wait
RUN chmod +x /wait

# Add library path to LD_LIBRARY_PATH
ENV LD_LIBRARY_PATH=/opt/belledonne-communications/lib:/usr/local/lib
ENV LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libjemalloc.so.2
ENV MALLOC_ARENA_MAX=2
ENV MALLOC_MMAP_THRESHOLD_=131072
ENV MALLOC_TRIM_THRESHOLD_=131072
ENV MALLOC_TOP_PAD_=131072
ENV MALLOC_MMAP_MAX_=65536

# Add it to the default path
ENV PATH=$PATH:/opt/belledonne-communications/bin

# Set the working directory
WORKDIR /opt/belledonne-communications

# Set the entrypoint
ENTRYPOINT ["/flexisip-entrypoint.sh"]

# Default command (can be overridden)
CMD ["flexisip", "--config", "/etc/flexisip/DEVOPS-32/flexisip.conf"]
