#!/bin/bash
#
# Build script for SandboxGE
# Usage: ./build.sh [clean|build|rebuild|run]
#   clean   - Remove build directory
#   build   - Configure and build (default)
#   rebuild - Clean then build
#   run     - Clean, build, and run demo
#

set -e

BUILD_DIR="${CMAKE_BUILD_DIR:-build}"
CONFIG="${CONFIG:-Release}"
DEMO_FLAG="${SANDBOX_GE_BUILD_DEMO:-ON}"
EXTRA_CMAKE_ARGS="${SANDBOX_GE_CMAKE_ARGS:-}"

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

ACTION="${1:-build}"

echo -e "${YELLOW}============================================${NC}"
echo -e "${YELLOW}SandboxGE - Linux Build${NC}"
echo -e "${YELLOW}============================================${NC}"
echo "Action: $ACTION"
echo "Build Dir: $BUILD_DIR"
echo "Config: $CONFIG"
echo "Demo: $DEMO_FLAG"
echo ""

do_clean() {
    echo -e "${YELLOW}[CLEAN] Removing $BUILD_DIR...${NC}"
    if [ -d "$BUILD_DIR" ]; then
        rm -rf "$BUILD_DIR"
        echo -e "${GREEN}✓ Build directory removed${NC}"
    else
        echo "Nothing to clean."
    fi
    echo ""
}

do_build() {
    echo -e "${YELLOW}[BUILD] Configuring with CMake...${NC}"
    
    # Auto-detect GLFW and math includes from cloth_solver if not set
    EXTRA_ARGS="$EXTRA_CMAKE_ARGS"
    if [ -z "$SANDBOX_GE_EXTRA_INCLUDE_DIRS" ] && [ -d "../cloth_solver/modules/math/include" ]; then
        EXTRA_ARGS="$EXTRA_ARGS -DSANDBOX_GE_EXTRA_INCLUDE_DIRS=../cloth_solver/modules/math/include"
    fi
    
    mkdir -p "$BUILD_DIR"
    
    cmake -S . -B "$BUILD_DIR" \
        -DCMAKE_BUILD_TYPE="$CONFIG" \
        -DSANDBOX_GE_BUILD_DEMO="$DEMO_FLAG" \
        -G Ninja \
        $EXTRA_ARGS
    
    echo -e "${YELLOW}[BUILD] Compiling...${NC}"
    cmake --build "$BUILD_DIR" --config "$CONFIG" --parallel "$(nproc)"
    
    echo -e "${GREEN}✓ Build successful${NC}"
    echo ""
}

copy_shaders() {
    SHADER_SRC="$(pwd)/shaders"
    if [ ! -d "$SHADER_SRC" ]; then
        echo -e "${YELLOW}⚠ Shader directory not found: $SHADER_SRC${NC}"
        return
    fi
    
    DEST="$BUILD_DIR/shaders"
    echo -e "${YELLOW}[BUILD] Copying shaders to $DEST...${NC}"
    mkdir -p "$DEST"
    cp -r "$SHADER_SRC"/* "$DEST/" 2>/dev/null || true
    echo -e "${GREEN}✓ Shaders copied${NC}"
}

find_demo_exe() {
    DEMO_EXE=""
    if [ -f "$BUILD_DIR/SandboxGE_Demo" ]; then
        DEMO_EXE="$BUILD_DIR/SandboxGE_Demo"
    fi
    echo "$DEMO_EXE"
}

do_run() {
    DEMO_EXE=$(find_demo_exe)
    if [ -z "$DEMO_EXE" ] || [ ! -f "$DEMO_EXE" ]; then
        echo -e "${RED}✗ Could not find SandboxGE_Demo executable${NC}"
        echo "Ensure SANDBOX_GE_BUILD_DEMO=ON and rebuild."
        exit 1
    fi
    
    echo -e "${YELLOW}[RUN] Launching demo...${NC}"
    echo ""
    "$DEMO_EXE"
}

# Process action
case "$ACTION" in
    clean)
        do_clean
        ;;
    build)
        do_build
        copy_shaders
        ;;
    rebuild)
        do_clean
        do_build
        copy_shaders
        ;;
    run)
        do_clean
        do_build
        copy_shaders
        do_run
        ;;
    *)
        echo -e "${RED}ERROR: Unknown action \"$ACTION\"${NC}"
        echo "Usage: ./build.sh [clean|build|rebuild|run]"
        exit 1
        ;;
esac

echo -e "${GREEN}============================================${NC}"
echo -e "${GREEN}Done${NC}"
echo -e "${GREEN}============================================${NC}"
