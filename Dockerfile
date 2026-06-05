FROM ubuntu:26.04

ENV DEBIAN_FRONTEND=noninteractive

# Install system dependencies
RUN apt-get update && apt-get install -y \
    wget \
    mold \
    ccache \
    cmake \
    lsb-release \
    software-properties-common \
    gnupg \
    git \
    curl \
    unzip \
    zip \
    pkg-config \
    dpkg-dev \
    rpm \
    file \
    g++-15 \
    && rm -rf /var/lib/apt/lists/*

# Install LLVM 21
RUN wget https://apt.llvm.org/llvm.sh && \
    chmod +x llvm.sh && \
    ./llvm.sh 21 && \
    apt-get update && \
    apt-get install -y clang-format-21 libc++-21-dev libc++abi-21-dev && \
    rm llvm.sh && \
    rm -rf /var/lib/apt/lists/* && \
    ln -sf /usr/bin/clang-21 /usr/bin/clang && \
    ln -sf /usr/bin/clang++-21 /usr/bin/clang++ && \
    ln -sf /usr/bin/clang-format-21 /usr/bin/clang-format

# Set up vcpkg
RUN git clone https://github.com/microsoft/vcpkg.git /opt/vcpkg && \
    /opt/vcpkg/bootstrap-vcpkg.sh -disableMetrics
ENV VCPKG_ROOT=/opt/vcpkg

# Install Catalyst Orchestrated Builder (COB)
RUN wget https://github.com/CatalystCPP/catalyst-orchestrated-builder/releases/download/0.5.0/cob-0.5.0.deb && \
    apt-get update && \
    apt-get install -y ./cob-0.5.0.deb && \
    rm cob-0.5.0.deb && \
    rm -rf /var/lib/apt/lists/*

# Install Catalyst (Bootstrap compiler)
RUN wget https://github.com/CatalystCPP/catalyst-build-system/releases/download/v1.4.3/catalyst-1.4.3.deb && \
    apt-get update && \
    apt-get install -y ./catalyst-1.4.3.deb && \
    rm catalyst-1.4.3.deb && \
    rm -rf /var/lib/apt/lists/*

# Install GitHub CLI (used by the release workflow)
RUN mkdir -p -m 755 /etc/apt/keyrings && \
    wget -nv -O- https://cli.github.com/packages/githubcli-archive-keyring.gpg | tee /etc/apt/keyrings/githubcli-archive-keyring.gpg > /dev/null && \
    chmod go+r /etc/apt/keyrings/githubcli-archive-keyring.gpg && \
    echo "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/githubcli-archive-keyring.gpg] https://cli.github.com/packages stable main" > /etc/apt/sources.list.d/github-cli.list && \
    apt-get update && \
    apt-get install -y gh && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /workspace

CMD ["/bin/bash"]
