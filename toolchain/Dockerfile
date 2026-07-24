FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive

# --- System Deps ---
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    git \
    clang \
    clang-format \
    clang-tidy \
    lld \
    python3 \
    python3-pip \
    curl \
    autoconf \
    flex \
    bison \
    libfl-dev \
    libfl2 \
    help2man \
    perl \
    gtkwave \
    x11-apps \
    && rm -rf /var/lib/apt/lists/*

ARG VERILATOR_VERSION=v5.048
RUN git clone --depth=1 --branch $VERILATOR_VERSION \
        https://github.com/verilator/verilator.git /tmp/verilator \
    && cd /tmp/verilator \
    && autoconf \
    && ./configure \
    && make -j$(nproc) \
    && make install \
    && rm -rf /tmp/verilator

# --- Non-root user matching the host UID/GID ---
ARG USER_UID=1000
ARG USER_GID=1000
RUN groupadd --gid "$USER_GID" dev \
    && useradd --uid "$USER_UID" --gid "$USER_GID" --create-home --shell /bin/bash dev \
    && apt-get update && apt-get install -y sudo \
    && echo "dev ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/dev \
    && rm -rf /var/lib/apt/lists/*

# --- Workspace ---
WORKDIR /workspace

# --- Build simforge ---
COPY .. .

RUN cmake -S . -B build -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER=clang \
        -DCMAKE_CXX_COMPILER=clang++ \
    && ninja -C build

RUN ln -s /workspace/build/tools/cli/simforge /usr/local/bin/simforge

RUN chown -R dev:dev /workspace

ENV GTK_THEME=Adwaita:dark

USER dev
WORKDIR /project

CMD ["/bin/bash"]
