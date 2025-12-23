# app/src/main/cpp/CMakeLists.txt

cmake_minimum_required(VERSION 3.22.1)
project("skiatestframework")

# 查找 NDK 包（可选，但推荐）
# find_package(ndk REQUIRED CONFIG)

# 创建 native-lib
add_library(
        native-lib
        SHARED
        AndroidOut.cpp
        OpenGLES/EglManager.cpp
        native-lib.cpp
        My_Renderer.cpp
        # old_native-lib.cpp
        # My_Old_Renderer.cpp
        OpenGLES/SkiaOpenGLPipeline.cpp
        SkiaPipeline.cpp
        Vulkan/SkiaVulkanPipeline.cpp
        Vulkan/VulkanManager.cpp
        Vulkan/VulkanSurface.cpp
)

# 添加 Skia 头文件路径
target_include_directories(native-lib PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/skia
)

# 导入预编译的 Skia 动态库
add_library(skia SHARED IMPORTED)
set_target_properties(skia PROPERTIES
        IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/../jniLibs/${ANDROID_ABI}/libskia.so
)

add_library(third_party__perfetto STATIC IMPORTED)
set_target_properties(third_party__perfetto PROPERTIES
        IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/../jniLibs/${ANDROID_ABI}/libperfetto.a
)


# 链接系统库 + Skia
target_link_libraries(
        native-lib
        android
        log
        EGL
        GLESv3
        skia
        vulkan
        third_party__perfetto
)

# 设置 Skia 编译宏（关键！来自知识库第五步）
target_compile_definitions(native-lib PRIVATE
        SKIA_DLL
        SK_ENABLE_SKSL
        SK_ENABLE_PRECOMPILE
        SK_ASSUME_GL_ES=1
        SK_GAMMA_APPLY_TO_A8
        SK_GAMMA_EXPONENT=1.4
        SK_GAMMA_CONTRAST=0.0
        SKIA_IMPLEMENTATION=1
        SK_GL
        SK_VULKAN
        SK_ENABLE_DUMP_GPU
        SK_SUPPORT_GPU=1
        SK_USE_PERFETTO
        SK_DEBUG
        SK_USE_VMA
        VK_USE_PLATFORM_ANDROID_KHR
)
