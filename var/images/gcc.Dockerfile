FROM gcc:latest
ARG CONFIGURE="-DCMAKE_BUILD_TYPE=Release"
ARG COMPILE="-j"
RUN apt update -y && apt upgrade -y
RUN apt install -y cmake \
    zlib1g-dev \
    libassimp-dev \
    libfreetype-dev \
    libsdl2-dev \
    libspirv-cross-c-shared-dev glslang-dev glslang-tools \
    libopenal-dev \
    libglew-dev \
    libssl-dev \
    libmongoc-dev libbson-dev \
    libpq-dev \
    libsqlite3-dev
RUN mkdir /home/target/ && mkdir /home/target/make
COPY ./ /home/target/intermediate
RUN cmake -S=/home/target/intermediate -B=/home/target/make -DCMAKE_LIBRARY_OUTPUT_DIRECTORY=/usr/local/lib -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=/usr/local/bin $CONFIGURE
RUN make -C /home/target/make $COMPILE
RUN make -C /home/target/make install
WORKDIR /usr/local/bin