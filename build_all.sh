#!/bin/bash
# build_all.sh — build barelog for all platforms using Docker

set -e

IMAGE="${IMAGE:-barelog:latest}"
PARALLEL="${PARALLEL:-1}"

echo "barelog multi-platform build"
echo "Image: $IMAGE"
echo "Parallel jobs: $PARALLEL"
echo ""

# Build Docker image if not exists
if ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
    echo "Building Docker image $IMAGE..."
    docker build -f Dockerfile_optimized -t "$IMAGE" .
    echo ""
fi

# Platforms to build
PLATFORMS=(
    "atmega328p"
    "ch32v203"
    "rp2040"
    "nrf52840"
    "esp32s3"
)

# Build function
build_platform() {
    local platform=$1
    echo "Building $platform..."
    docker run --rm \
        -v "$(pwd):/workspace" \
        "$IMAGE" \
        make PLATFORM="$platform" size
    echo "✓ $platform done"
}

# Export function for parallel execution
export -f build_platform
export IMAGE

# Run builds
if [ "$PARALLEL" -eq 1 ]; then
    # Sequential
    for platform in "${PLATFORMS[@]}"; do
        build_platform "$platform"
    done
else
    # Parallel
    printf '%s\n' "${PLATFORMS[@]}" | \
        xargs -I {} -P "$PARALLEL" bash -c 'build_platform "$@"' _ {}
fi

echo ""
echo "All platforms built successfully!"
echo ""
echo "Outputs in:"
for platform in "${PLATFORMS[@]}"; do
    echo "  build_$platform/barelog.hex (or .bin)"
done
