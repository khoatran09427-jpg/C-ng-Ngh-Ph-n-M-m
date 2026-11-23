FROM ubuntu:24.04

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    qt6-base-dev \
    libgl1-mesa-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY . .

RUN cmake -S . -B build-docker \
    && cmake --build build-docker

CMD ["./build-docker/VehicleRentalSystem"]
