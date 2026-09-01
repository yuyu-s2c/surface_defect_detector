// ONNX Runtime + MinGW 链接验证 demo（Phase 2 风险点预验证，见 DEVELOPMENT.md）。
// 用法：
//   ort_demo.exe                -> 只打印 ORT 版本（验证编译/链接/dll 加载）
//   ort_demo.exe <model.onnx>   -> 额外创建推理会话并跑一次全零输入（验证推理链路）
//
// 链接方式：直接用 lib/onnxruntime.dll 作为链接输入（GNU ld 支持对 dll 自动生成
// 导入项），不依赖 MSVC 的 .lib，绕开 MinGW 链接 MSVC 导入库的不确定性。
#include <onnxruntime_cxx_api.h>

#include <cstdio>
#include <string>

// Windows 下 ORTCHAR_T 为 wchar_t；demo 路径按 ASCII 处理即可
static std::wstring toWide(const char* s)
{
    return std::wstring(s, s + std::strlen(s));
}

int main(int argc, char* argv[])
{
    // 1) C API 基础调用：验证头文件编译 + dll 链接 + 运行期加载
    const OrtApiBase* base = OrtGetApiBase();
    if (!base) {
        std::fprintf(stderr, "FAIL: OrtGetApiBase returned null\n");
        return 1;
    }
    std::printf("ORT version: %s\n", base->GetVersionString());

    if (argc < 2) {
        std::printf("OK (link + load). 未提供模型，跳过推理验证。\n");
        return 0;
    }

    // 2) 创建会话并跑一次推理：验证真实模型链路
    try {
        Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "ort_demo");
        Ort::SessionOptions opts;
        opts.SetIntraOpNumThreads(1);
        Ort::Session session(env, toWide(argv[1]).c_str(), opts);

        Ort::AllocatorWithDefaultOptions alloc;
        const auto inName = session.GetInputNameAllocated(0, alloc);
        const auto outName = session.GetOutputNameAllocated(0, alloc);
        const auto inInfo = session.GetInputTypeInfo(0).GetTensorTypeAndShapeInfo();
        const std::vector<int64_t> shape = inInfo.GetShape();

        size_t count = 1;
        std::printf("input: name=%s shape=[", inName.get());
        for (size_t i = 0; i < shape.size(); ++i) {
            std::printf("%s%lld", i ? "," : "", static_cast<long long>(shape[i]));
            count *= (shape[i] > 0 ? shape[i] : 1); // 动态维按 1 占位
        }
        std::printf("]\n");

        std::vector<float> input(count, 0.0f);
        Ort::MemoryInfo mem = Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPU);
        Ort::Value inTensor = Ort::Value::CreateTensor<float>(
            mem, input.data(), input.size(), shape.data(), shape.size());

        const char* inNames[] = { inName.get() };
        const char* outNames[] = { outName.get() };
        auto outputs = session.Run(Ort::RunOptions{}, inNames, &inTensor, 1, outNames, 1);
        const auto outShape = outputs[0].GetTensorTypeAndShapeInfo().GetShape();
        std::printf("output: name=%s shape=[", outName.get());
        for (size_t i = 0; i < outShape.size(); ++i)
            std::printf("%s%lld", i ? "," : "", static_cast<long long>(outShape[i]));
        std::printf("]\nOK (inference)\n");
    } catch (const Ort::Exception& e) {
        std::fprintf(stderr, "FAIL: %s\n", e.what());
        return 1;
    }
    return 0;
}
