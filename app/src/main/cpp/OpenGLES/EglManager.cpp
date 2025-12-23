
#include <string>
#include <unordered_set>
#include <vector>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <assert.h>

#include "../AndroidOut.h"
#include "EglManager.h"

#define ERROR_CASE(x) \
    case x:           \
        return #x;
static const char* egl_error_str(EGLint error) {
    switch (error) {
        ERROR_CASE(EGL_SUCCESS)
        ERROR_CASE(EGL_NOT_INITIALIZED)
        ERROR_CASE(EGL_BAD_ACCESS)
        ERROR_CASE(EGL_BAD_ALLOC)
        ERROR_CASE(EGL_BAD_ATTRIBUTE)
        ERROR_CASE(EGL_BAD_CONFIG)
        ERROR_CASE(EGL_BAD_CONTEXT)
        ERROR_CASE(EGL_BAD_CURRENT_SURFACE)
        ERROR_CASE(EGL_BAD_DISPLAY)
        ERROR_CASE(EGL_BAD_MATCH)
        ERROR_CASE(EGL_BAD_NATIVE_PIXMAP)
        ERROR_CASE(EGL_BAD_NATIVE_WINDOW)
        ERROR_CASE(EGL_BAD_PARAMETER)
        ERROR_CASE(EGL_BAD_SURFACE)
        ERROR_CASE(EGL_CONTEXT_LOST)
        default:
            return "Unknown error";
    }
}
const char* EglManager::eglErrorString() {
    return egl_error_str(eglGetError());
}

class unordered_string_set : public std::unordered_set<std::string> {
public:
    bool has(const char* str) { return find(std::string(str)) != end(); }
};

class StringUtils {
public:
    static unordered_string_set split(const char* spacedList) {
        unordered_string_set set;
        const char* current = spacedList;
        const char* head = current;
        do {
            head = strchr(current, ' ');
            std::string s(current, head ? head - current : strlen(current));
            if (s.length()) {
                set.insert(std::move(s));
            }
            current = head + 1;
        } while (head);
        return set;
    }
};

static struct {
    bool bufferAge = false;
    bool setDamage = false;
    bool noConfigContext = false;
    bool pixelFormatFloat = false;
    bool glColorSpace = false;
    bool scRGB = false;
    bool displayP3 = false;
    bool hdr = false;
    bool contextPriority = false;
    bool surfacelessContext = false;
    bool nativeFenceSync = false;
    bool fenceSync = false;
    bool waitSync = false;
} EglExtensions;

EglManager::EglManager()
        : mEglDisplay(EGL_NO_DISPLAY)
        , mEglConfig(EGL_NO_CONFIG_KHR)
        , mEglConfigF16(EGL_NO_CONFIG_KHR)
        , mEglConfig1010102(EGL_NO_CONFIG_KHR)
        , mEglContext(EGL_NO_CONTEXT)
        , mPBufferSurface(EGL_NO_SURFACE)
        , mCurrentSurface(EGL_NO_SURFACE)
{}

EglManager::~EglManager() {
    destroy();
    if (hasEglContext()) {
        aout << "EglManger leaked an EGL context" << std::endl;
    }
}

void EglManager::initialize() {
    if (hasEglContext())
        return;

    //获取Display
    mEglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);

    EGLint major, minor;
    eglInitialize(mEglDisplay, &major, &minor);

    initExtensions();

    loadConfig();
    createContext();
    createPBufferSurface();
    makeCurrent(mPBufferSurface, nullptr, true);

}

void EglManager::initExtensions() {
    auto extensions = StringUtils::split(eglQueryString(mEglDisplay, EGL_EXTENSIONS));

    EglExtensions.bufferAge =
            extensions.has("EGL_EXT_buffer_age") || extensions.has("EGL_KHR_partial_update");
    EglExtensions.setDamage = extensions.has("EGL_KHR_partial_update");
    EglExtensions.glColorSpace = extensions.has("EGL_KHR_gl_colorspace");
    EglExtensions.noConfigContext = extensions.has("EGL_KHR_no_config_context");
    EglExtensions.pixelFormatFloat = extensions.has("EGL_EXT_pixel_format_float");
    EglExtensions.scRGB = extensions.has("EGL_EXT_gl_colorspace_scrgb");
    EglExtensions.displayP3 = extensions.has("EGL_EXT_gl_colorspace_display_p3_passthrough");
    EglExtensions.hdr = extensions.has("EGL_EXT_gl_colorspace_bt2020_pq");
    EglExtensions.contextPriority = extensions.has("EGL_IMG_context_priority");
    EglExtensions.surfacelessContext = extensions.has("EGL_KHR_surfaceless_context");
    EglExtensions.fenceSync = extensions.has("EGL_KHR_fence_sync");
    EglExtensions.waitSync = extensions.has("EGL_KHR_wait_sync");
    EglExtensions.nativeFenceSync = extensions.has("EGL_ANDROID_native_fence_sync");
}

void EglManager::loadConfig() {
    //加载8 Bit Config
    mEglConfig = load8BitsConfig(mEglDisplay);

    //加载float16 config
    if (EglExtensions.pixelFormatFloat) {
        mEglConfigF16 = loadFP16Config(mEglDisplay);
        if (mEglConfigF16 == EGL_NO_CONFIG_KHR) {
            aout << "Device claims wide gamut support, cannot find matching config" << std::endl;
            EglExtensions.pixelFormatFloat = false;
        }
    }

    //加载1010102 config
    mEglConfig1010102 = load1010102Config(mEglDisplay);
    if (mEglConfig1010102 == EGL_NO_CONFIG_KHR) {
        aout << "Failed to initialize 1010102 format" << std::endl;
    }
}

void EglManager::createContext() {
    std::vector<EGLint> contextAttributes;
    contextAttributes.reserve(5);
    contextAttributes.push_back(EGL_CONTEXT_CLIENT_VERSION);
    contextAttributes.push_back(3);
    contextAttributes.push_back(EGL_NONE);
    mEglContext = eglCreateContext(
            mEglDisplay, EglExtensions.noConfigContext ? ((EGLConfig) nullptr) : mEglConfig,
            EGL_NO_CONTEXT, contextAttributes.data());

    if (mEglContext == EGL_NO_CONTEXT) {
        aout << "Failed to create context" << std::endl;
    }
}

void EglManager::createPBufferSurface() {
    assert(mEglDisplay != EGL_NO_DISPLAY && "Uninitialized");

    if (mPBufferSurface == EGL_NO_SURFACE && !EglExtensions.surfacelessContext) {
        EGLint attribs[] = {EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE};
        mPBufferSurface = eglCreatePbufferSurface(mEglDisplay, mEglConfig, attribs);
        if (mPBufferSurface == EGL_NO_SURFACE) {
            aout << "Failed to create a pixel buffer display" << std::endl;
        }
    }
}

EGLConfig EglManager::load8BitsConfig(EGLDisplay display) {

    //尝试加载 GLES3.2
    EGLint attribs[] = {EGL_RENDERABLE_TYPE,
                        EGL_OPENGL_ES3_BIT,
                        EGL_RED_SIZE,
                        8,
                        EGL_GREEN_SIZE,
                        8,
                        EGL_BLUE_SIZE,
                        8,
                        EGL_ALPHA_SIZE,
                        8,
                        EGL_DEPTH_SIZE,
                        0,
                        EGL_CONFIG_CAVEAT,
                        EGL_NONE,
                        EGL_STENCIL_SIZE,
                        8,
                        EGL_SURFACE_TYPE,
                        EGL_WINDOW_BIT,
                        EGL_NONE};

    EGLConfig config = EGL_NO_CONFIG_KHR;
    EGLint numConfigs = 1;
    if (!eglChooseConfig(display, attribs, &config, numConfigs, &numConfigs) || numConfigs != 1) {
        return EGL_NO_CONFIG_KHR;
    }

    return config;
}

EGLConfig EglManager::loadFP16Config(EGLDisplay display) {
    EGLint attribs[] = {EGL_RENDERABLE_TYPE,
                        EGL_OPENGL_ES3_BIT,
                        EGL_COLOR_COMPONENT_TYPE_EXT,
                        EGL_COLOR_COMPONENT_TYPE_FLOAT_EXT,
                        EGL_RED_SIZE,
                        16,
                        EGL_GREEN_SIZE,
                        16,
                        EGL_BLUE_SIZE,
                        16,
                        EGL_ALPHA_SIZE,
                        16,
                        EGL_DEPTH_SIZE,
                        0,
                        EGL_STENCIL_SIZE,
                        8,
                        EGL_SURFACE_TYPE,
                        EGL_WINDOW_BIT ,
                        EGL_NONE};
    EGLConfig config = EGL_NO_CONFIG_KHR;
    EGLint numConfigs = 1;
    if (!eglChooseConfig(display, attribs, &config, numConfigs, &numConfigs) || numConfigs != 1) {
        return EGL_NO_CONFIG_KHR;
    }
    return config;
}

EGLConfig EglManager::load1010102Config(EGLDisplay display) {
    EGLint attribs[] = {EGL_RENDERABLE_TYPE,
                        EGL_OPENGL_ES3_BIT,
                        EGL_RED_SIZE,
                        10,
                        EGL_GREEN_SIZE,
                        10,
                        EGL_BLUE_SIZE,
                        10,
                        EGL_ALPHA_SIZE,
                        2,
                        EGL_DEPTH_SIZE,
                        0,
                        EGL_STENCIL_SIZE,
                        8,
                        EGL_SURFACE_TYPE,
                        EGL_WINDOW_BIT,
                        EGL_NONE};
    EGLConfig config = EGL_NO_CONFIG_KHR;
    EGLint numConfigs = 1;
    if (!eglChooseConfig(display, attribs, &config, numConfigs, &numConfigs) || numConfigs != 1) {
        return EGL_NO_CONFIG_KHR;
    }
    return config;
}

EGLConfig EglManager::loadA8Config(EGLDisplay display) {
    EGLint attribs[] = {EGL_RENDERABLE_TYPE,
                        EGL_OPENGL_ES2_BIT,
                        EGL_RED_SIZE,
                        8,
                        EGL_GREEN_SIZE,
                        0,
                        EGL_BLUE_SIZE,
                        0,
                        EGL_ALPHA_SIZE,
                        0,
                        EGL_DEPTH_SIZE,
                        0,
                        EGL_SURFACE_TYPE,
                        EGL_WINDOW_BIT,
                        EGL_NONE};
    EGLint numConfigs = 1;
    if (!eglChooseConfig(display, attribs, nullptr, numConfigs, &numConfigs)) {
        return EGL_NO_CONFIG_KHR;
    }

    std::vector<EGLConfig> configs(numConfigs, EGL_NO_CONFIG_KHR);
    if (!eglChooseConfig(display, attribs, configs.data(), numConfigs, &numConfigs)) {
        return EGL_NO_CONFIG_KHR;
    }


    for (EGLConfig config : configs) {
        EGLint r{0}, g{0}, b{0}, a{0};
        eglGetConfigAttrib(display, config, EGL_RED_SIZE, &r);
        eglGetConfigAttrib(display, config, EGL_GREEN_SIZE, &g);
        eglGetConfigAttrib(display, config, EGL_BLUE_SIZE, &b);
        eglGetConfigAttrib(display, config, EGL_ALPHA_SIZE, &a);
        if (8 == r && 0 == g && 0 == b && 0 == a) {
            return config;
        }
    }
    return EGL_NO_CONFIG_KHR;
}

bool EglManager::swapBuffers(const GLSurface& frame) {
    eglSwapBuffers(mEglDisplay, frame.mSurface);

    EGLint err = eglGetError();
    if (err == EGL_SUCCESS) {
        return true;
    }

    if (err == EGL_BAD_SURFACE || err == EGL_BAD_NATIVE_WINDOW) {
        aout << "swapbuffers encountered egl error" << std::endl;
        return false;
    }

    return false;
}

GLSurface EglManager::beginFrame(EGLSurface surface) {
    assert( surface != EGL_NO_SURFACE && "Tried to beginFrame on EGL_NO_SURFACE");

    makeCurrent(surface);
    GLSurface frame;
    frame.mSurface = surface;
    eglQuerySurface(mEglDisplay, surface, EGL_WIDTH, &frame.mWidth);
    eglQuerySurface(mEglDisplay, surface, EGL_HEIGHT, &frame.mHeight);

    return frame;
}

bool EglManager::makeCurrent(EGLSurface surface, EGLint* errOut, bool force) {
    if (!force && isCurrent(surface)) return false;

    if (surface == EGL_NO_SURFACE) {
        surface = mPBufferSurface;
    }
    if (!eglMakeCurrent(mEglDisplay, surface, surface, mEglContext)) {
        if (errOut) {
            *errOut = eglGetError();
            aout << "Failed to make current on surface error: " << egl_error_str(*errOut) << std::endl;
        } else {
            aout << "Failed to make current on surface error: " << egl_error_str(*errOut) << std::endl;
        }
    }
    mCurrentSurface = surface;

    return true;
}

EGLSurface EglManager::createSurface(EGLNativeWindowType window, ColorMode colorMode, sk_sp<SkColorSpace> colorSpace) {
    assert(hasEglContext() && "Not initialized");
    if (!EglExtensions.noConfigContext) {
        colorMode = ColorMode::Default;
    }

    EGLint attribs[] = { EGL_NONE, EGL_NONE, EGL_NONE};

    EGLConfig config = mEglConfig;
    if (colorMode == ColorMode::A8) {
        if (!mEglConfigA8) {
            mEglConfigA8 = loadA8Config(mEglDisplay);
        }

        config = mEglConfigA8;
    } else {
        colorMode = ColorMode::Default;

        if (EglExtensions.glColorSpace) {
            attribs[0] = EGL_GL_COLORSPACE_KHR;
            switch (colorMode) {
                case ColorMode::Default:
                    attribs[1] = EGL_GL_COLORSPACE_LINEAR_KHR;
                    break;
                default:
                    aout << "Unreachable: unsupported color mode" << std::endl;
            }
        }
    }

    EGLSurface surface = eglCreateWindowSurface(mEglDisplay, config, window, attribs);
    if (surface == EGL_NO_SURFACE) {
        return nullptr;
    }

    return surface;
}


void EglManager::destroySurface(EGLSurface surface) {
    if (isCurrent(surface)) {
        makeCurrent(EGL_NO_SURFACE);
    }

    if (!eglDestroySurface(mEglDisplay, surface)) {
        aout << "Failed to destroy surface" << std::endl;
    }
}

void EglManager::destroy() {
    if (mEglDisplay == EGL_NO_DISPLAY) return;

    eglDestroyContext(mEglDisplay, mEglContext);
    if (mPBufferSurface != EGL_NO_SURFACE) {
        eglDestroySurface(mEglDisplay, mPBufferSurface);
    }

    eglMakeCurrent(mEglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglTerminate(mEglDisplay);
    eglReleaseThread();

    mEglDisplay = EGL_NO_DISPLAY;
    mEglContext = EGL_NO_CONTEXT;
    mPBufferSurface = EGL_NO_SURFACE;
    mCurrentSurface = EGL_NO_SURFACE;
}

bool EglManager::hasEglContext() {
    return mEglDisplay != EGL_NO_DISPLAY;
}
