#pragma once
#include <FFGLSDK.h>

namespace control {
// Reusable image adapter. No networking, file IO, or parameter work during Draw.
class PassThrough {
    struct State {
        GLint program, active, texture, vao, buffer;
        GLboolean blend, depth, scissor, mask[4];
        State() {
            glGetIntegerv(GL_CURRENT_PROGRAM, &program);
            glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
            glActiveTexture(GL_TEXTURE0);
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
            glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
            glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &buffer);
            blend = glIsEnabled(GL_BLEND); depth = glIsEnabled(GL_DEPTH_TEST); scissor = glIsEnabled(GL_SCISSOR_TEST);
            glGetBooleanv(GL_COLOR_WRITEMASK, mask);
        }
        ~State() {
            glUseProgram(program); glBindVertexArray(vao); glBindBuffer(GL_ARRAY_BUFFER, buffer);
            glBindTexture(GL_TEXTURE_2D, texture); glActiveTexture(active);
            Set(GL_BLEND, blend); Set(GL_DEPTH_TEST, depth); Set(GL_SCISSOR_TEST, scissor);
            glColorMask(mask[0], mask[1], mask[2], mask[3]);
        }
        static void Set(GLenum capability, GLboolean enabled) { if (enabled) glEnable(capability); else glDisable(capability); }
    };
public:
    bool Init() {
        State saved;
        const char* vertex = "#version 410 core\nuniform vec2 MaxUV; layout(location=0) in vec4 vPosition; layout(location=1) in vec2 vUV; out vec2 uv; void main(){gl_Position=vPosition;uv=vUV*MaxUV;}";
        const char* fragment = "#version 410 core\nuniform sampler2D InputTexture; in vec2 uv; out vec4 fragColor; void main(){fragColor=texture(InputTexture,uv);}";
        if (!shader.Compile(vertex, fragment) || !quad.Initialise()) { Free(); return false; }
        return true;
    }
    FFResult Draw(ProcessOpenGLStruct* input) {
        if (!input || input->numInputTextures < 1 || !input->inputTextures || !input->inputTextures[0]) return FF_FAIL;
        State saved;
        glDisable(GL_BLEND); glDisable(GL_DEPTH_TEST); glDisable(GL_SCISSOR_TEST);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glUseProgram(shader.GetGLID()); glBindTexture(GL_TEXTURE_2D, input->inputTextures[0]->Handle);
        shader.Set("InputTexture", 0);
        const auto uv = GetMaxGLTexCoords(*input->inputTextures[0]);
        shader.Set("MaxUV", uv.s, uv.t);
        quad.Draw();
        return FF_SUCCESS;
    }
    void Free() { shader.FreeGLResources(); quad.Release(); }
private:
    ffglex::FFGLShader shader;
    ffglex::FFGLScreenQuad quad;
};
}
