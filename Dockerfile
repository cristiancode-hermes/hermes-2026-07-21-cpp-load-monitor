# Stage 1: Build
FROM gcc:14-bookworm AS builder
RUN apt-get update && apt-get install -y cmake ninja-build
WORKDIR /src
COPY CMakeLists.txt CMakePresets.json ./
COPY src/ include/ tests/ ./
RUN cmake --preset cli-only && cmake --build --preset cli-only

# Stage 2: Runtime (~25 MB)
FROM debian:bookworm-slim
RUN apt-get update && apt-get install -y ca-certificates && rm -rf /var/lib/apt/lists/*
COPY --from=builder /src/build/cli/load-monitor /usr/local/bin/load-monitor
ENTRYPOINT ["load-monitor"]
CMD ["--no-tui"]
