FROM ubuntu:18.04

RUN apt-get update && apt-get -y install \
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
        python

ADD https://dl.google.com/dl/cloudsdk/channels/rapid/downloads/google-cloud-sdk-173.0.0-linux-x86_64.tar.gz /tmp/gcloud.tar.gz
RUN tar -C /usr/local/ -xzf /tmp/gcloud.tar.gz
RUN /usr/local/google-cloud-sdk/install.sh

ENV bazel_version 0.25.2
RUN wget --quiet -O /tmp/bazel-${bazel_version}-installer-linux-x86_64.sh \
  "https://github.com/bazelbuild/bazel/releases/download/${bazel_version}/bazel-${bazel_version}-installer-linux-x86_64.sh" && \
  chmod +x "/tmp/bazel-${bazel_version}-installer-linux-x86_64.sh"
RUN /tmp/bazel-${bazel_version}-installer-linux-x86_64.sh
