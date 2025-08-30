# 构建脚本

ARCH=$(uname -m)
OS=$(uname -s)

USE_APPLE_ACCELERATE=OFF
USE_MKL=OFF
USE_CPUS=1

if [ $OS == "Darwin" ]; then
    if ! command -v xcodebuild &> /dev/null; then
        echo "xcodebuild could not be found"
        exit 1
    fi

    if ! command -v brew &> /dev/null; then
        echo "brew could not be found"
        exit 1
    fi

    ### 安装依赖
    if ! command -v cmake &> /dev/null; then
        brew install cmake ccache git gperftools pcre2 openblas
    fi

    if [ $ARCH == "arm64" ]; then
        USE_APPLE_ACCELERATE=ON
    fi

    USE_CPUS=$(sysctl -n hw.ncpu)
elif [ $OS == "Linux" ]; then
    USE_MKL=ON
    USE_CPUS=$(nproc)
fi

rm -rf build
mkdir -p build && cd build

cmake .. \
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    -DCMAKE_BUILD_TYPE=Release \
    -DUSE_STATIC_LIBS=ON \
    -DCOMPILE_CUDA=OFF \
    -DCOMPILE_CPU=ON \
    -DCOMPILE_EXAMPLES=ON \
    -DUSE_CCACHE=ON \
    -DUSE_APPLE_ACCELERATE=$USE_APPLE_ACCELERATE \
    -DUSE_MKL=$USE_MKL \

make -j$USE_CPUS
