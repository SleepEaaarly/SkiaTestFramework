//
// Created by zeng on 2025/10/22.
//

#include <memory>

#include "My_Renderer.h"
#include "OpenGLES/SkiaOpenGLPipeline.h"
#include <android/native_window.h>
#include <android/log.h>
#include "vulkan/SkiaVulkanPipeline.h"

#define LOGI(...)  __android_log_print(ANDROID_LOG_INFO,  "SkiaDemo", __VA_ARGS__)
#define LOGE(...)  __android_log_print(ANDROID_LOG_ERROR, "SkiaDemo", __VA_ARGS__)

#define TRACE()    LOGI(">>> %s:%d  %s", __FILE_NAME__, __LINE__, __func__)

//#include "src/core/SkATrace.h"
//#include "tools/trace/SkDebugfTracer.h"
//#include "tools/trace/ChromeTracingTracer.h"
//#include "tools/trace/SkPerfettoTrace.h"
//
//static bool hasInitTracer = false;
//
//static void my_initializeEventTracingForTools(const char* traceFlag) {
//    hasInitTracer = true;
//    SkEventTracer* eventTracer = nullptr;
//    if (0 == strcmp(traceFlag, "atrace")) {
//        eventTracer = new SkATrace();
//    } else if (0 == strcmp(traceFlag, "debugf")) {
//        eventTracer = new SkDebugfTracer();
//    } else if (0 == strcmp(traceFlag, "perfetto")) {
//#if defined(SK_USE_PERFETTO)
//        eventTracer = new SkPerfettoTrace();
//#else
//        SkDebugf("Perfetto is not enabled (SK_USE_PERFETTO is false). Perfetto tracing will not "
//                 "be performed.\nTracing tools with Perfetto is only enabled for Linux, Android, "
//                 "and Mac.\n");
//        return;
//#endif
//    }
//    else {
//        eventTracer = new ChromeTracingTracer(traceFlag);
//    }
//
//    SkAssertResult(SkEventTracer::SetInstance(eventTracer));
//}

My_Renderer::My_Renderer(ANativeWindow *window) {
    init(window);
    // if (!hasInitTracer) my_initializeEventTracingForTools("perfetto");
}

void My_Renderer::render() {

    return mPipeline->draw();
}

void My_Renderer::init(ANativeWindow* window) {
    TRACE();
    mPipeline = std::make_unique<SkiaVulkanPipeline>();
    mPipeline->setSurface(window);
    TRACE();
}


void My_Renderer::setContext(sk_sp<GrDirectContext> context) {
    mGrContext = std::move(context);
}
