FROM ubuntu:26.04

ARG DEBIAN_FRONTEND=noninteractive
ARG LLVM_VERSION=21
ARG VCPKG_REF=2025.07.25
ARG COB_VERSION=0.5.0
ARG CATALYST_VERSION=2.0.0

# Use a persistent apt cache instead of deleting lists every layer
RUN rm -f /etc/apt/apt.conf.d/docker-clean && \
    echo 'Binary::apt::APT::Keep-Downloaded-Packages "true";' \
        > /etc/apt/apt.conf.d/keep-cache

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked \
    apt-get update && apt-get install -y --no-install-recommends \
        wget curl git unzip zip file gnupg ccache \
        cmake mold pkg-config dpkg-dev rpm \
        lsb-release software-properties-common \
        g++-15 ca-certificates

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked \
    wget -qO /tmp/llvm.sh https://apt.llvm.org/llvm.sh && \
    chmod +x /tmp/llvm.sh && /tmp/llvm.sh "${LLVM_VERSION}" && \
    apt-get update && apt-get install -y --no-install-recommends \
        clang-format-${LLVM_VERSION} \
        libc++-${LLVM_VERSION}-dev \
        libc++abi-${LLVM_VERSION}-dev && \
    rm /tmp/llvm.sh && \
    update-alternatives --install /usr/bin/clang clang /usr/bin/clang-${LLVM_VERSION} 100 && \
    update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-${LLVM_VERSION} 100 && \
    update-alternatives --install /usr/bin/clang-format clang-format /usr/bin/clang-format-${LLVM_VERSION} 100

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked \
    mkdir -p -m 755 /etc/apt/keyrings && \
    wget -nv -O- https://cli.github.com/packages/githubcli-archive-keyring.gpg \
        | tee /etc/apt/keyrings/githubcli-archive-keyring.gpg > /dev/null && \
    chmod go+r /etc/apt/keyrings/githubcli-archive-keyring.gpg && \
    echo "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/githubcli-archive-keyring.gpg] https://cli.github.com/packages stable main" \
        > /etc/apt/sources.list.d/github-cli.list && \
    apt-get update && apt-get install -y --no-install-recommends gh

ENV VCPKG_ROOT=/opt/vcpkg
RUN git clone --depth 1 --branch "${VCPKG_REF}" \
        https://github.com/microsoft/vcpkg.git /opt/vcpkg && \
    /opt/vcpkg/bootstrap-vcpkg.sh -disableMetrics

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked \
    wget -qO /tmp/cob.deb \
        https://github.com/CatalystCPP/catalyst-orchestrated-builder/releases/download/${COB_VERSION}/cob-${COB_VERSION}.deb && \
    wget -qO /tmp/catalyst.deb \
        https://github.com/CatalystCPP/catalyst-build-system/releases/download/v${CATALYST_VERSION}/catalyst-${CATALYST_VERSION}.deb && \
    apt-get update && \
    apt-get install -y /tmp/cob.deb /tmp/catalyst.deb && \
    rm /tmp/cob.deb /tmp/catalyst.deb

WORKDIR /workspace
CMD ["/bin/bash"]
