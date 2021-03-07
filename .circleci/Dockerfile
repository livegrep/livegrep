FROM ubuntu:20.04

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get -y install \
        build-essential \
        libxml2-utils \
        wget \
        pkg-config \
        zip \
        unzip \
        zlib1g-dev \
        openjdk-8-jdk \
        git \
        openssh-client \
        python python3

ADD https://dl.google.com/dl/cloudsdk/channels/rapid/downloads/google-cloud-sdk-268.0.0-linux-x86_64.tar.gz /tmp/gcloud.tar.gz
RUN tar -C /usr/local/ -xzf /tmp/gcloud.tar.gz
RUN /usr/local/google-cloud-sdk/install.sh

ENV bazel_version 4.0.0
RUN wget --quiet -O /tmp/bazel-${bazel_version}-installer-linux-x86_64.sh \
  "https://github.com/bazelbuild/bazel/releases/download/${bazel_version}/bazel-${bazel_version}-installer-linux-x86_64.sh" && \
  chmod +x "/tmp/bazel-${bazel_version}-installer-linux-x86_64.sh"
RUN /tmp/bazel-${bazel_version}-installer-linux-x86_64.sh
