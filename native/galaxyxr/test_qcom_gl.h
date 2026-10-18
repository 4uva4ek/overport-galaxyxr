// Minimal host mock for the opt-in GLES adapter. Not part of Android builds.
#pragma once
using EGLContext=void*; using EGLDisplay=void*; using EGLConfig=void*;
using GLuint=unsigned; using GLenum=unsigned; using GLint=int; using GLfloat=float;
constexpr EGLContext EGL_NO_CONTEXT=nullptr;
constexpr GLenum GL_NUM_EXTENSIONS=0x821D, GL_EXTENSIONS=0x1F03, GL_NO_ERROR=0;
constexpr GLenum GL_TEXTURE_BINDING_2D_ARRAY=0x8C1D, GL_TEXTURE_2D_ARRAY=0x8C1A;
static EGLContext eglGetCurrentContext();
static void (*eglGetProcAddress(const char*))();
static void glGetIntegerv(GLenum,GLint*);
static const unsigned char* glGetStringi(GLenum,GLuint);
static GLenum glGetError();
static void glBindTexture(GLenum,GLuint);
static void glGetTexParameteriv(GLenum,GLenum,GLint*);
static void glTexParameteri(GLenum,GLenum,GLint);
static void glTexParameterf(GLenum,GLenum,GLfloat);
