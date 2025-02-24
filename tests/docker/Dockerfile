# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
FROM ubuntu:impish-20211102

ENV TZ="UTC" \
    DEBIAN_FRONTEND=noninteractive

# Install dependencies of Java client builds.
RUN apt update && \
    apt install -y \
      build-essential \
      default-jdk \
      git \
      maven && \
    rm -rf /var/lib/apt/lists/*

# Install kafka streams examples.  This is a very slow step (tens of minutes), doing
# many maven dependency downloads without any parallelism.  To avoid re-running it
# on unrelated changes in other steps, this step is as early on the Dockerfile as possible.
RUN git -C /opt clone --branch ducktape2 https://github.com/redpanda-data/kafka-streams-examples.git && \
    cd /opt/kafka-streams-examples && mvn -DskipTests=true clean package

# Install our in-tree Java test clientst
COPY --chown=0:0 tests/java /tmp/java
RUN mvn clean package --batch-mode --file /tmp/java/kafka-verifier --define buildDir=/opt/kafka-verifier/
RUN mvn clean package --batch-mode --file /tmp/java/compacted-log-verifier --define buildDir=/opt/compacted-log-verifier
RUN mvn clean package --batch-mode --file /tmp/java/tx-verifier --define buildDir=/opt/tx-verifier
RUN mvn clean package --batch-mode --file /tmp/java/e2e-verifiers --define buildDir=/opt/e2e-verifiers

# - install distro-packaged depedencies
# - allow user env variables in ssh sessions
# - install dependencies of 'rpk debug' system scan
RUN apt update && \
    apt install -y \
      bind9-utils \
      bind9-dnsutils \
      bsdmainutils \
      curl \
      dmidecode \
      cmake \
      iproute2 \
      iptables \
      libatomic1 \
      libyajl-dev \
      libsasl2-dev \
      libssl-dev \
      net-tools \
      lsof \
      pciutils \
      nodejs \
      npm \
      openssh-server \
      python3-pip && \
    rm -rf /var/lib/apt/lists/* && \
    echo 'PermitUserEnvironment yes' >> /etc/ssh/sshd_config && \
    echo 'UsePAM yes' >> /etc/ssh/sshd_config && \
    echo 'root soft nofile 65535' >> /etc/security/limits.conf && \
    echo 'root hard nofile 65535' >> /etc/security/limits.conf

# install go
RUN mkdir -p /usr/local/go/ && \
    bash -c 'if [[ $(uname -m) = "aarch64" ]]; then ARCHID="arm64"; else export ARCHID="amd64"; fi && \
    curl -sSLf --retry 3 --retry-connrefused --retry-delay 2 https://golang.org/dl/go1.17.linux-${ARCHID}.tar.gz | tar -xz -C /usr/local/go/ --strip 1'
ENV PATH="${PATH}:/usr/local/go/bin"

# install kafka binary dependencies, librdkafka dev, kcat and kaf tools
ENV KAFKA_MIRROR="https://s3-us-west-2.amazonaws.com/kafka-packages"
RUN mkdir -p "/opt/kafka-2.3.1" && chmod a+rw /opt/kafka-2.3.1 && curl -s "$KAFKA_MIRROR/kafka_2.12-2.3.1.tgz" | tar xz --strip-components=1 -C "/opt/kafka-2.3.1" && \
    mkdir -p "/opt/kafka-2.4.1" && chmod a+rw /opt/kafka-2.4.1 && curl -s "$KAFKA_MIRROR/kafka_2.12-2.4.1.tgz" | tar xz --strip-components=1 -C "/opt/kafka-2.4.1" && \
    mkdir -p "/opt/kafka-2.5.0" && chmod a+rw /opt/kafka-2.5.0 && curl -s "$KAFKA_MIRROR/kafka_2.12-2.5.0.tgz" | tar xz --strip-components=1 -C "/opt/kafka-2.5.0" && \
    mkdir -p "/opt/kafka-2.7.0" && chmod a+rw /opt/kafka-2.7.0 && curl -s "$KAFKA_MIRROR/kafka_2.12-2.7.0.tgz" | tar xz --strip-components=1 -C "/opt/kafka-2.7.0" && \
    mkdir /opt/librdkafka && \
    curl -SL "https://github.com/edenhill/librdkafka/archive/v1.8.0.tar.gz" | tar -xz --strip-components=1 -C /opt/librdkafka && \
    cd /opt/librdkafka && \
    ./configure && \
    make -j$(nproc) && \
    make install && \
    cd /opt/librdkafka/tests && \
    make build -j$(nproc) && \
    go get github.com/birdayz/kaf/cmd/kaf && \
    mv /root/go/bin/kaf /usr/local/bin/ && \
    mkdir /tmp/kcat && \
    curl -SL "https://github.com/edenhill/kcat/archive/1.7.0.tar.gz" | tar -xz --strip-components=1 -C /tmp/kcat && \
    cd /tmp/kcat && \
    ./configure && \
    make -j$(nproc) && \
    make install && \
    ldconfig

# Install golang dependencies for kafka clients such as sarama
RUN git -C /opt clone https://github.com/Shopify/sarama.git && \
    cd /opt/sarama/examples/interceptors && go mod tidy && go build && \
    cd /opt/sarama/examples/http_server && go mod tidy && go build && \
    cd /opt/sarama/examples/consumergroup && go mod tidy && go build && \
    cd /opt/sarama/examples/sasl_scram_client && go mod tidy && go build

# Install our in-tree go tests
COPY --chown=0:0 tests/go /tmp/go
RUN cd /tmp/go/sarama/produce_test && go mod tidy && go build

# Install franz-go
RUN git -C /opt clone https://github.com/twmb/franz-go.git && cd /opt/franz-go && \
    cd /opt/franz-go/examples/bench && go mod tidy && go build

RUN go install github.com/twmb/kcl@latest && \
    mv /root/go/bin/kcl /usr/local/bin/

# Should fork to redpanda repo
RUN git -C /opt clone https://github.com/jcsp/si-verifier.git && \
    cd /opt/si-verifier && git reset --hard bcf4996d063e96497f8ff8be35d82fa1cec53180 && \
    go mod tidy && go build

# Expose port 8080 for any http examples within clients
EXPOSE 8080

# copy ssh keys
COPY --chown=0:0 tests/docker/ssh /root/.ssh

# install python dependencies and rptest package.
# rptest package installed in editable mode so it can be overridden.
# passes --force so system pip packages can be updated
COPY --chown=0:0 tests/setup.py /root/tests/
RUN python3 -m pip install --upgrade --force pip && \
    python3 -m pip install --force --no-cache-dir -e /root/tests/

# copy python tools
COPY --chown=0:0 tests/python /opt/python

CMD service ssh start && tail -f /dev/null
