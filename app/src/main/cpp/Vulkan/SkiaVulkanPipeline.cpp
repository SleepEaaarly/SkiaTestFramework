//
// Created by zeng on 2025/11/26.
//

#include "SkiaVulkanPipeline.h"
#include "include/core/SkSurface.h"
#include "include/core/SkColorSpace.h"
#include "include/core/SkCanvas.h"

SkiaVulkanPipeline::SkiaVulkanPipeline()
        : fSurfaceColorType(SkColorType::kN32_SkColorType),
        fSurfaceColorSpace(SkColorSpace::MakeSRGB()) {

}

void SkiaVulkanPipeline::requireVkContext() {
    if (VulkanManager::getInstance().hasVkContext()) {
        return;
    }

    VulkanManager::getInstance().initialize();

    // create Skia GrDirectContext
    GrContextOptions options;
    // 如果ES3 context 报告了ES2的外部图像扩展支持，则强制把shader转换为ES2.0 的 shader language
    options.fPreferExternalImagesOverES3 = true;
    // 是否关闭SDF路径渲染（SDF的生成非常耗时，所以只有在渲染以不同的transform渲染相同的Path时此优化才有作用)
    options.fDisableDistanceFieldPaths = true;
    // 指示Skia是否采用更激进的策略来减少render pass的数量（离屏渲染会提前进行而不是打断主要的render pass)
    options.fReduceOpsTaskSplitting = GrContextOptions::Enable::kYes;
    // 是否开启 Path Mask纹理的缓存
    options.fAllowPathMaskCaching = true;

    // auto vkDriverVersion = VulkanManager::getInstance().getDriverVersion();
    sk_sp<GrDirectContext> grContext = VulkanManager::getInstance().createContext(options);
    setGrContext(grContext);
}

bool SkiaVulkanPipeline::setSurface(ANativeWindow* window) {
    fNativeWindow = window;
    if (fVkSurface) {
        VulkanManager::getInstance().destroySurface(fVkSurface);
    }

    if (window) {
        requireVkContext();
        fVkSurface = VulkanManager::getInstance().
                createSurface(window, fColorMode, fSurfaceColorSpace, fSurfaceColorType, fGrContext->asDirectContext(), 0);
    }

    return fVkSurface != nullptr;
}

void SkiaVulkanPipeline::draw() {
    sk_sp<SkSurface> backBuffer;

    backBuffer = fVkSurface->getBackbufferSurface();
    SkCanvas* canvas = backBuffer->getCanvas();

    renderFrame(canvas);

    fVkSurface->swapBuffers();
}


static void simple_test(SkCanvas* canvas) {
    SkPaint paint;
    paint.setStyle(SkPaint::kFill_Style);
    paint.setAntiAlias(true);
    paint.setStrokeWidth(4);
    paint.setColor(0xff4285F4);

    SkRect rect = SkRect::MakeXYWH(10, 10, 100, 160);
    canvas->drawRect(rect, paint);
}

void SkiaVulkanPipeline::renderFrame(SkCanvas* canvas) {
    canvas->clear(SK_ColorWHITE);
    simple_test(canvas);
}
