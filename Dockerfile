FROM ubuntu:24.04 AS builder

ARG DEBIAN_FRONTEND=noninteractive
ARG BUILD_GUI=ON

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    ca-certificates \
    cmake \
    g++ \
    libgtest-dev \
    libssl-dev \
    ninja-build \
    qt6-base-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

RUN cmake -S /src -B /tmp/build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTS=ON \
    -DBUILD_GUI=${BUILD_GUI}

RUN cmake --build /tmp/build --parallel

RUN ctest --test-dir /tmp/build --output-on-failure

RUN set -eux; \
    mkdir -p /out/bin /out/tests; \
    cp /tmp/build/target_exec /out/bin/target_exec; \
    if [ -f /tmp/build/messenger_gui ]; then cp /tmp/build/messenger_gui /out/bin/messenger_gui; fi; \
    find /tmp/build/tests -maxdepth 1 -type f -perm /111 -exec cp {} /out/tests/ \;


FROM ubuntu:24.04 AS runtime

ARG DEBIAN_FRONTEND=noninteractive
ARG BUILD_GUI=ON

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    libssl3 \
    qt6-base-dev \
    && rm -rf /var/lib/apt/lists/*

RUN useradd --create-home --shell /bin/bash messenger

WORKDIR /home/messenger/app

COPY --from=builder /out/bin/ /usr/local/bin/
COPY --from=builder /out/tests/ /usr/local/lib/evlampy-tests/

COPY docker/entrypoint.sh /usr/local/bin/entrypoint.sh
RUN chmod +x /usr/local/bin/entrypoint.sh

USER messenger
ENV HOME=/home/messenger

ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
CMD []

