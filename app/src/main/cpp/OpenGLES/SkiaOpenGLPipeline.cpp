//
// Created by zeng on 2025/10/22.
//

#include "SkiaOpenGLPipeline.h"

#include "include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "include/gpu/ganesh/GrBackendSurface.h"
#include "include/core/SkSurface.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include <GLES3/gl3.h>
#include "include/gpu/ganesh/gl/GrGLInterface.h"
#include "include/gpu/ganesh/gl/GrGLDirectContext.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkBlurTypes.h"
#include "include/core/SkMaskFilter.h"
#include "include/core/SkBitmap.h"

#include <android/log.h>
#include <android/trace.h>

#define LOGI(...)  __android_log_print(ANDROID_LOG_INFO,  "SkiaDemo", __VA_ARGS__)
#define LOGE(...)  __android_log_print(ANDROID_LOG_ERROR, "SkiaDemo", __VA_ARGS__)

static bool has_gaussian = false;

static void example_render(SkCanvas* canvas) {
    const SkScalar w = canvas->imageInfo().width();
    const SkScalar h = canvas->imageInfo().height();
    const SkScalar centerX = w * 0.5f;
    const SkScalar centerY = h * 0.5f;

    // 矩形尺寸（固定，也可按画布比例缩放）
    const SkScalar rectW = w * 0.35f;
    const SkScalar rectH = h * 0.2f;
    SkRect rect = SkRect::MakeXYWH(-rectW * 0.5f, -rectH * 0.5f, rectW, rectH);

    // 每帧角度 +1°
    static float gDegrees = 0.0f;
    gDegrees += 1.0f;
    if (gDegrees >= 360.0f) gDegrees -= 360.0f;

    SkMatrix matrix;
    matrix.setRotate(gDegrees, 0, 0);   // 绕矩形中心(0,0)旋转
    matrix.postTranslate(centerX, centerY);

    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeWidth(8);
    paint.setColor(SkColorSetARGB(255, 0, 120, 255));

    canvas->save();
    canvas->concat(matrix);
    canvas->drawRect(rect, paint);
    canvas->restore();
}

static void gaussian_blur_key_test(SkCanvas* canvas) {
    if (has_gaussian)   return ;
    ATrace_beginSection("gs_blur_test");

    // 确保 canvas matrix 是 identity
    canvas->setMatrix(SkMatrix::I());

    SkPaint paint;
    paint.setMaskFilter(SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, 100.0f));
    paint.setStyle(SkPaint::kFill_Style);

    SkPath path;
    path.addRoundRect(SkRect::MakeXYWH(60, 60, 1200, 800), 120, 120);

    const int N = 200; // 绘制大量相似图形

    for (int i = 0; i < N; ++i) {
        // 微小变化：平移 0.01 * i，sigma = 10.0 + 0.01 * i
        float sigma = 100.0f + 0.0001f * i;

        // 构造 viewMatrix：微小平移 + 微小缩放（可选）
        SkMatrix viewMatrix = SkMatrix::I();
        // 可选：加微小缩放，如 1.0 + 0.0001*i
        viewMatrix.preScale(1.0f + 0.0001f * i, 1.0f);

        // 设置 canvas matrix（模拟变换）
        canvas->setMatrix(viewMatrix);

        // 设置 blur filter（sigma 微变）
        paint.setMaskFilter(SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, sigma));

        // 绘制路径（位置固定，但 canvas matrix 变了）
        canvas->drawPath(path, paint);
    }
    has_gaussian = true;
    ATrace_endSection();
}

static void blit_row_color_test() {
    sk_sp<SkSurface> surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(1920,
                                                                             1080));
    if (!surface) {
        LOGE("blit_row_color_test surface alloc error");
        return ;
    }

    SkCanvas* canvas = surface->getCanvas();
    SkPaint p;
    p.setColor(SkColorSetARGB(128, 255, 0, 0));   // alpha ∈ (0,255)
    p.setBlendMode(SkBlendMode::kSrcOver);        // 默认即可

    ATrace_beginSection("blit_row_color_test");
    canvas->drawRect(SkRect::MakeWH(1920, 1080), p);
    ATrace_endSection();
}

#include "include/core/SkRRect.h"

static void circle_clip_test(SkCanvas* canvas) {
    // 1. 准备一张 2×2 的 RGBA 纹理（任意内容即可）
    const SkImageInfo info = SkImageInfo::MakeN32Premul(2, 2);
    sk_sp<SkSurface> surf = SkSurfaces::Raster(info);
    surf->getCanvas()->clear(SK_ColorRED);          // 随便填点颜色
    sk_sp<SkImage> img = surf->makeImageSnapshot();

    // 2. 构造一个正方形，圆角半径 = 边长/2 → 退化成圆
    const float side = 100.f;
    SkRect dst = SkRect::MakeXYWH(100, 100, side, side);
    SkRRect rr = SkRRect::MakeRectXY(dst, side/2, side/2);   // 关键：半径=50

    // 3. 用 clip + 抗锯齿 触发 GPU 圆角路径
    canvas->clipRRect(rr, true);          // 第二个参数 = doAA
    SkPaint paint;
    paint.setAntiAlias(true);             // 必须

    // 4. 关键调用：drawImageRect
    //    系统会把 dst 转成一个带圆角的裁剪区域，
    //    从而触发 GrRRectEffect → GrOvalEffect → Circle 路径
    canvas->drawImageRect(img.get(),
                          SkRect::MakeWH(2, 2),   // src 矩形
                          dst,                     // dst 圆角矩形
                          SkSamplingOptions(SkFilterMode::kLinear),
                          &paint,
                          SkCanvas::kStrict_SrcRectConstraint);
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

void SkiaOpenGLPipeline::renderFrame(SkCanvas* canvas) {
    canvas->clear(SK_ColorWHITE);          // 清屏
    simple_test(canvas);
    // gaussian_blur_key_test(canvas);
    // blit_row_color_test();
    // circle_clip_test(canvas);
}

SkiaOpenGLPipeline::SkiaOpenGLPipeline()
        : mSurfaceColorType(SkColorType::kN32_SkColorType)
        , mSurfaceColorSpace(SkColorSpace::MakeSRGB()) {

    //TODO: support more colorspace & colortype

    //创建EGL环境
    mEglManager = std::make_unique<EglManager>();

}

void SkiaOpenGLPipeline::draw() {
    GLSurface frame = mEglManager->beginFrame(mEglSurface);
    if (frame.width() <= 0 || frame.height() <= 0) return;

    EGLContext current = eglGetCurrentContext();
    if (current == EGL_NO_CONTEXT) return;

    GrGLFramebufferInfo fboInfo;
    fboInfo.fFBOID = 0;
    fboInfo.fFormat = GL_RGBA8;   // 与下面 colorType 匹配

    GrBackendRenderTarget backendRT =
            GrBackendRenderTargets::MakeGL(frame.width(), frame.height(),
                                           0, 8, fboInfo);  // sampleCount=0 即可

    sk_sp<SkSurface> surface = SkSurfaces::WrapBackendRenderTarget(
            fGrContext.get(), backendRT, kBottomLeft_GrSurfaceOrigin,
            kRGBA_8888_SkColorType, mSurfaceColorSpace, nullptr);

    if (!surface) return;

    SkCanvas* canvas = surface->getCanvas();
    renderFrame(canvas);

    fGrContext->asDirectContext()->flushAndSubmit();
    mEglManager->swapBuffers(frame);
}

bool SkiaOpenGLPipeline::setSurface(ANativeWindow* surface) {
    if (mEglSurface != EGL_NO_SURFACE) {
        mEglManager->destroySurface(mEglSurface);
        mEglSurface = EGL_NO_SURFACE;
    }

    if (surface) {
        requireGlContext();

        auto newSurface = mEglManager->createSurface(surface, mColorMode, mSurfaceColorSpace);
        if (!newSurface) {
            return false;
        }

        mEglSurface = newSurface;
    }

    return mEglSurface != EGL_NO_SURFACE;
}

void SkiaOpenGLPipeline::requireGlContext() {
    if (mEglManager->hasEglContext()) {
        return;
    }

    mEglManager->initialize();

    //创建Skia GrDirectContext
    GrContextOptions options;
    //如果ES3 context 报告了ES2的外部图像扩展支持，则强制把shader转换为ES2.0 的 shader language
//    options.fPreferExternalImagesOverES3 = true;
//    //是否关闭SDF路径渲染（SDF的生成非常耗时，所以只有在渲染以不同的transform渲染相同的Path时此优化才有作用)
//    options.fDisableDistanceFieldPaths = true;
//    //指示Skia是否采用更激进的策略来减少render pass的数量（离屏渲染会提前进行而不是打断主要的render pass)
//    options.fReduceOpsTaskSplitting = GrContextOptions::Enable::kYes;
//    //是否开启 Path Mask纹理的缓存
//    options.fAllowPathMaskCaching = true;
    //创建Context
    sk_sp<const GrGLInterface> glInterface = GrGLMakeNativeInterface();
    sk_sp<GrDirectContext> grContext(GrDirectContexts::MakeGL(std::move(glInterface), options));
    setGrContext(grContext);
}


