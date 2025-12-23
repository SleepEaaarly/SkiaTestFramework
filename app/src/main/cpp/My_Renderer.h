//
// Created by zeng on 2025/10/22.
//

#ifndef SKIAOPENGLES_MY_RENDERER_H
#define SKIAOPENGLES_MY_RENDERER_H

#include "SkiaPipeline.h"

#include "OpenGLES/GLSurface.h"

class ANativeWindow;

class My_Renderer {
public:
    My_Renderer(ANativeWindow* window);

    virtual ~My_Renderer() = default;

    void render();

private:
    void init(ANativeWindow* window);

    void setContext(sk_sp<GrDirectContext> context);

    sk_sp<GrDirectContext> mGrContext;
    std::unique_ptr<SkiaPipeline> mPipeline;
};


#endif //SKIAOPENGLES_MY_RENDERER_H
