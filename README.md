# AI 编译与转换工具箱

基于 GitHub Actions 的自动化编译和转换系统，支持 PaddleOCR C++ 库编译和 Qwen 模型 ONNX 转换。

## 功能特性

### 多平台支持

支持以下目标平台的编译：
- **Linux x64**: 生成 `.so` 共享库（适用于 Ubuntu/Debian 等）
- **Linux ARM64**: 生成 `.so` 共享库（适用于树莓派、ARM 服务器等）
- **Windows x64**: 生成 `.dll` 动态链接库
- **Android ARM64**: 生成 `.so` 共享库（适用于 Android 应用）

### 1. PaddleOCR C++ 编译
自动化编译 PaddleOCR C++ 推理库，支持高度自定义的编译参数。

**支持的配置**：
- 多种硬件架构（x86_64、ARM/AArch64）
- 多种加速方案（CPU、GPU、TensorRT）
- 自定义 Paddle 版本和依赖库路径
- 完整的 CMake 参数控制

**输出产物**：包含 `.so` 库文件和 `ocr_system` 二进制文件的 tarball。

### 2. Qwen 模型转换
将 Qwen (v2/v2.5) 模型转换为优化的 ONNX 格式。

**支持的选项**：
- 多种量化类型（无量化、int8、int4）
- 优化级别控制
- GPU/CPU 转换选择
- 批次大小配置

**输出产物**：ONNX 模型文件和 tokenizer。

---

## 🚀 快速开始

### PaddleOCR 自动化编译

本工具箱支持一次点击生产多平台产物：

1. 进入仓库的 **Actions** 标签页。
2. 选择 **🛠️ Multi-Platform Builder** 工作流。
3. 点击 **Run workflow**。
4. 配置参数：
   - **编译模式**：Release 或 Debug。
   - **Paddle 预测库版本**：默认 2.5.0。
5. 运行并等待完成。
6. 从 **Artifacts** 区域下载产物：
   - `ocr-android-lib`: 包含 `.so` 文件。
   - `ocr-windows-dll`: 包含 `.dll` 文件。

### Qwen (LLM) 自动化编译

使用 `llama.cpp` 高性能推理引擎：

1. 进入仓库的 **Actions** 标签页。
2. 选择 **🤖 Qwen (llama.cpp) Builder** 工作流。
3. 点击 **Run workflow**。
4. 配置参数：
   - **编译模式**：Release 或 Debug。
   - **Windows: 启用 CUDA 加速**：勾选以启用 GPU 支持。
5. 运行并等待完成。
6. 从 **Artifacts** 区域下载产物：
   - `qwen-android-lib`: 包含 `libllama.so` 和 `llama-cli`（Android 可执行文件）。
   - `qwen-windows-bin`: 包含 `llama-cli.exe` 和依赖的 `.dll`。

---

## 📂 项目结构

```text
.
├── .github/workflows/
│   └── paddle_build.yml    # GitHub Actions 核心逻辑
├── scripts/
│   ├── build_android.sh        # PaddleOCR Android 编译脚本
│   ├── build_windows.ps1       # PaddleOCR Windows 编译脚本
│   ├── build_qwen_android.sh   # Qwen Android 编译脚本
│   └── build_qwen_windows.ps1  # Qwen Windows 编译脚本
├── CMakeLists.txt          # 核心 CMake 配置文件
├── 开发计划.md              # 工程化设计文档
└── README.md               # 本文档
```

---

## 高级用法

### 平台特定编译

#### Linux x64（标准 CPU）
```yaml
target_platform: "linux-x64"
paddle_version: "2.6.0"
with_mkl: true
```
产物：`paddleocr_cpp_linux-x64.tar.gz`（包含 `.so` 文件）

#### Windows x64
```yaml
target_platform: "windows-x64"
paddle_version: "2.6.0"
with_mkl: true
```
产物：`paddleocr_cpp_windows-x64.zip`（包含 `.dll` 文件）

#### Android ARM64
```yaml
target_platform: "android-arm64"
paddle_version: "2.6.0"
with_mkl: false
```
产物：`paddleocr_cpp_android-arm64.tar.gz`（包含 `.so` 文件）

#### Linux ARM64（树莓派等）
```yaml
target_platform: "linux-arm64"
paddle_version: "2.6.0"
with_mkl: false
```
产物：`paddleocr_cpp_linux-arm64.tar.gz`（包含 `.so` 文件）

### PaddleOCR 自定义编译

#### 启用 GPU 支持
```yaml
with_gpu: true
cuda_lib: "/usr/local/cuda/lib64"
cudnn_lib: "/usr/local/cuda/lib64"
```

#### 启用 TensorRT
```yaml
with_tensorrt: true
tensorrt_dir: "/usr/local/TensorRT"
```

#### ARM 交叉编译
```yaml
with_arm: true
extra_cmake_flags: "-DCMAKE_TOOLCHAIN_FILE=/path/to/arm-toolchain.cmake"
```

#### 自定义 Paddle 版本
```yaml
paddle_version: "2.5.0"
paddle_url: "https://custom-url/paddle_inference.tgz"
```

### Qwen 模型转换选项

#### int8 量化（推荐用于 CPU 部署）
```yaml
quantization_type: "int8"
optimization_level: 2
```

#### GPU 加速转换
```yaml
use_gpu: true
```

---

## CMake 参数参考

| 参数 | 类型 | 说明 | 默认值 |
|------|------|------|--------|
| `WITH_GPU` | BOOL | 启用 GPU 支持 | OFF |
| `WITH_MKL` | BOOL | 启用 MKL 加速 | ON |
| `WITH_TENSORRT` | BOOL | 启用 TensorRT | OFF |
| `WITH_ARM` | BOOL | ARM 交叉编译 | OFF |
| `WITH_STATIC_LIB` | BOOL | 静态链接 | OFF |
| `CUDA_LIB` | PATH | CUDA 库路径 | - |
| `CUDNN_LIB` | PATH | cuDNN 库路径 | - |
| `TENSORRT_DIR` | PATH | TensorRT 路径 | - |
| `OPENCV_DIR` | PATH | OpenCV 路径 | 自动检测 |

---

## 本地开发

### PaddleOCR 本地编译

```bash
# 安装依赖
cd paddleocr/scripts
chmod +x install_deps.sh
./install_deps.sh --paddle-version 2.6.0

# 编译（默认配置）
chmod +x build.sh
./build.sh -DWITH_MKL=ON -DWITH_GPU=OFF

# 编译（启用 GPU）
./build.sh -DWITH_GPU=ON -DWITH_MKL=ON -DCUDA_LIB=/usr/local/cuda/lib64
```

### Qwen 本地转换

```bash
# 安装依赖
cd qwen
pip install -r requirements.txt

# 转换模型（无量化）
python convert.py --model_id Qwen/Qwen2.5-0.5B-Instruct --output_dir output

# 转换模型（int8 量化）
python convert.py \
  --model_id Qwen/Qwen2.5-0.5B-Instruct \
  --output_dir output \
  --quantization_type int8 \
  --optimization_level 2
```

---

## 常见问题

### Q: 如何选择 Paddle 版本？
A: 访问 [Paddle Inference 发布页](https://www.paddlepaddle.org.cn/inference/master/guides/install/download_lib.html) 查看可用版本。

### Q: 量化会影响模型精度吗？
A: int8 量化会有轻微精度损失（通常 <1%），int4 量化损失较大。建议先测试无量化版本作为基准。

### Q: 如何验证编译产物？
A: 下载 tarball 后解压，检查是否包含 `.so` 文件和可执行文件。可在 Ubuntu 环境中运行 `ldd` 检查依赖。

### Q: 转换失败怎么办？
A: 检查模型 ID 是否正确，确保网络可访问 Hugging Face。查看 Actions 日志获取详细错误信息。

---

## 贡献指南

欢迎提交 Issue 和 Pull Request！

---

## 许可证

MIT License

---

## 相关资源

- [PaddleOCR 官方文档](https://github.com/PaddlePaddle/PaddleOCR)
- [Qwen 模型库](https://huggingface.co/Qwen)
- [Optimum 文档](https://huggingface.co/docs/optimum)
- [完整开发计划](./开发计划.md)
