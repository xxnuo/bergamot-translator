# 构建脚本

## macOS
if [ "$(uname)" == "Darwin" ]; then

### 构建环境
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
brew install cmake ccache git \
    gperftools pcre2 \
    doxygen graphviz
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
    -DUSE_DOXYGEN=ON \
    -DUSE_APPLE_ACCELERATE=ON \
    -DUSE_MKL=OFF \
    -DUSE_SIMD_UTILS=ON \

### 构建
make -j$(sysctl -n hw.ncpu)

fi
