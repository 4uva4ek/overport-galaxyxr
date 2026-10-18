// SPDX-License-Identifier: GPL-3.0-or-later
// Opt-in prototype: full-rect stereo GLES arrays only. No runtime META profile.
#if defined(__ANDROID__) || defined(GXR_QCOM_TEST)
#ifdef GXR_QCOM_TEST
#include "test_qcom_gl.h"
#else
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#endif
#include <deque>
#include <chrono>
#include "android_eye_compat.h"
#include "qcom_math.h"
struct QcomBinding { XrStructureType type; const void* next; EGLDisplay display; EGLConfig config; EGLContext context; };
struct QcomImage { XrStructureType type; void* next; GLuint image; };
struct QcomSession {
    EGLContext context = EGL_NO_CONTEXT;
    XrEyeTrackerANDROID tracker = XR_NULL_HANDLE;
    XrSpace space = XR_NULL_HANDLE;
    PFN_xrGetFineTrackingEyesInfoANDROID getEyes = nullptr;
    XrTime time = 0;
    std::chrono::steady_clock::time_point sampled{};
    bool running = false;
    std::chrono::steady_clock::time_point diagnosticStarted = std::chrono::steady_clock::now();
    bool glChecked = false, glSupported = false;
    bool focalValid = false, holding = false;
    std::chrono::steady_clock::time_point lastGood{};
    int gazeResult=0, gazeMode=0, eyeStates[2]{}; uint32_t viewCount=0; XrViewStateFlags viewFlags=0;
    float focalX[2]{}, focalY[2]{};
};
struct QcomChain {
    XrSession session;
    uint32_t width, height;
    uint32_t requestedSamples = 1;
    std::vector<QcomImage> images;
    std::deque<uint32_t> acquired;
    bool mapped = false;
    uint64_t frames = 0;
    int diagnosticLast = -1;
    float diagnosticX[2]{}, diagnosticY[2]{};
};
static std::mutex qcomMutex;
static std::unordered_map<XrSession,QcomSession> qcomSessions;
static std::unordered_map<XrSwapchain,QcomChain> qcomChains;
static void qcomSessionCreated(XrSession session, const XrSessionCreateInfo* info) {
    if (!qcomFoveation() && !runtimeEyeFoveation()) return;
    auto p = sessionPolicy(session);
    if (!p.qcom || !p.gles) return;
    QcomSession s;
    for (auto n = static_cast<const XrBaseInStructure*>(info->next); n; n=n->next)
        if (n->type==XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR)
            s.context=reinterpret_cast<const QcomBinding*>(n)->context;
    XrInstance instance;
    { std::lock_guard<std::mutex> lock(stateMutex); instance=sessions.at(session).instance; }
    XrSystemEyeTrackingPropertiesANDROID support{XR_TYPE_SYSTEM_EYE_TRACKING_PROPERTIES_ANDROID};
    XrSystemProperties properties{XR_TYPE_SYSTEM_PROPERTIES, &support};
    auto getProperties=proc<PFN_xrGetSystemProperties>(instance,"xrGetSystemProperties");
    if(!getProperties || getProperties(instance,info->systemId,&properties)!=XR_SUCCESS || !support.supportsEyeTracking) { LOG("QCOM gaze unavailable: system does not support eye tracking"); return; }
    auto create = proc<PFN_xrCreateEyeTrackerANDROID>(instance,"xrCreateEyeTrackerANDROID");
    s.getEyes = proc<PFN_xrGetFineTrackingEyesInfoANDROID>(instance,"xrGetFineTrackingEyesInfoANDROID");
    auto createSpace = symbol<PFN_xrCreateReferenceSpace>("xrCreateReferenceSpace");
    XrEyeTrackerCreateInfoANDROID ci{XR_TYPE_EYE_TRACKER_CREATE_INFO_ANDROID};
    auto result = create && s.getEyes ? create(session,&ci,&s.tracker) : XR_ERROR_FUNCTION_UNSUPPORTED;
    XrReferenceSpaceCreateInfo si{XR_TYPE_REFERENCE_SPACE_CREATE_INFO,nullptr,XR_REFERENCE_SPACE_TYPE_VIEW,{{0,0,0,1},{0,0,0}}};
    auto spaceResult = createSpace ? createSpace(session,&si,&s.space) : XR_ERROR_FUNCTION_UNSUPPORTED;
    LOG("QCOM gaze init tracker=%d space=%d (no META profile)",result,spaceResult);
    std::lock_guard<std::mutex> lock(qcomMutex); qcomSessions[session]=s;
}
static void qcomRunning(XrSession session, bool running) {
    std::lock_guard<std::mutex> lock(qcomMutex);
    auto it=qcomSessions.find(session); if(it!=qcomSessions.end()) { it->second.lastGood={}; it->second.focalValid=false; it->second.holding=false; it->second.running=running; it->second.time=0; it->second.diagnosticStarted=std::chrono::steady_clock::now(); }
}
static void qcomForget(XrSession session) {
    // Called only after runtime destruction; child tracker/space handles are implicit children.
    std::lock_guard<std::mutex> lock(qcomMutex);
    qcomSessions.erase(session);
    for(auto it=qcomChains.begin();it!=qcomChains.end();) if(it->second.session==session) it=qcomChains.erase(it); else ++it;
}
static void qcomFrame(XrSession session, const XrFrameState* state) {
    if(!state) return;
    std::lock_guard<std::mutex> lock(qcomMutex);
    auto it=qcomSessions.find(session); if(it==qcomSessions.end()) return;
    it->second.time=state->shouldRender ? state->predictedDisplayTime : 0;
    it->second.sampled=std::chrono::steady_clock::now();
    auto& s=it->second;
    s.focalValid=false; s.holding=false;
    if(runtimeEyeFoveation() && s.running && s.time>0 && s.tracker && s.space && s.getEyes) {
        XrEyesANDROID eyes{XR_TYPE_EYES_ANDROID};
        XrEyesGetInfoANDROID get{XR_TYPE_EYES_GET_INFO_ANDROID,nullptr,s.time,s.space};
        auto gaze=s.getEyes(s.tracker,&get,&eyes);
        XrView views[2]{{XR_TYPE_VIEW},{XR_TYPE_VIEW}};
        XrViewState vs{XR_TYPE_VIEW_STATE}; uint32_t count=0;
        XrViewLocateInfo locate{XR_TYPE_VIEW_LOCATE_INFO,nullptr,XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,s.time,s.space};
        auto locateViews=symbol<PFN_xrLocateViews>("xrLocateViews");
        bool valid=gaze==XR_SUCCESS && eyes.mode==XR_EYE_TRACKING_MODE_BOTH_ANDROID && locateViews &&
            locateViews(session,&locate,&vs,2,&count,views)==XR_SUCCESS && count==2 &&
            (vs.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT);
        bool viewValid=valid;
        float nextX[2]{},nextY[2]{};
        for(int i=0;valid && i<2;++i) valid=eyes.eyes[i].eyeState==XR_EYE_STATE_GAZING_ANDROID &&
            qcomProject(eyes.eyes[i].eyePose.orientation,views[i],nextX[i],nextY[i]);
        bool shut=eyes.eyes[0].eyeState==XR_EYE_STATE_SHUT_ANDROID || eyes.eyes[1].eyeState==XR_EYE_STATE_SHUT_ANDROID;
        bool known=(eyes.eyes[0].eyeState==XR_EYE_STATE_GAZING_ANDROID || eyes.eyes[0].eyeState==XR_EYE_STATE_SHUT_ANDROID) &&
                   (eyes.eyes[1].eyeState==XR_EYE_STATE_GAZING_ANDROID || eyes.eyes[1].eyeState==XR_EYE_STATE_SHUT_ANDROID);
        if(valid) {
            for(int i=0;i<2;++i) { s.focalX[i]=nextX[i]; s.focalY[i]=nextY[i]; }
            s.lastGood=s.sampled;
        } else if(viewValid && shut && known && s.lastGood.time_since_epoch().count()!=0 &&
                  s.sampled-s.lastGood<std::chrono::milliseconds(300)) {
            valid=true; s.holding=true; // Do not extend lastGood on held samples.
        } else s.lastGood={};
        s.gazeResult=gaze; s.gazeMode=eyes.mode; s.eyeStates[0]=eyes.eyes[0].eyeState; s.eyeStates[1]=eyes.eyes[1].eyeState;
        s.viewCount=count; s.viewFlags=vs.viewStateFlags;
        s.focalValid=valid;
    } else s.lastGood={};
}
static void qcomChainCreated(XrSession session,const XrSwapchainCreateInfo* info,XrSwapchain chain) {
    if((!qcomFoveation() && !runtimeEyeFoveation()) || !info) return;
    std::lock_guard<std::mutex> lock(qcomMutex);
    if(!qcomSessions.count(session)) return;
    bool candidate=info->arraySize==2 && info->faceCount==1 && info->mipCount==1 && (info->sampleCount==1 || info->sampleCount==2) &&
        info->width>=512 && info->height>=512 && (info->usageFlags & XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT) &&
        !(info->usageFlags & XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) && !(info->createFlags & XR_SWAPCHAIN_CREATE_STATIC_IMAGE_BIT);
    LOG("QCOM candidate=%d array=%u samples=%u size=%ux%u",candidate,info->arraySize,info->sampleCount,info->width,info->height);
    if(!candidate) return;
    auto enumerate=symbol<PFN_xrEnumerateSwapchainImages>("xrEnumerateSwapchainImages");
    uint32_t count=0;
    if(!enumerate || enumerate(chain,0,&count,nullptr)!=XR_SUCCESS || !count || count>16) return;
    QcomChain c{session,info->width,info->height};
    c.requestedSamples=info->sampleCount;
    c.images.resize(count,{XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR,nullptr,0});
    if(enumerate(chain,count,&count,reinterpret_cast<XrSwapchainImageBaseHeader*>(c.images.data()))!=XR_SUCCESS || count>c.images.size()) return;
    c.images.resize(count); qcomChains.emplace(chain,std::move(c));
}
static bool qcomOwns(XrSwapchain chain) { std::lock_guard<std::mutex> lock(qcomMutex); return qcomFoveation() && qcomChains.count(chain)!=0; }
static void qcomChainDestroyed(XrSwapchain chain) { std::lock_guard<std::mutex> lock(qcomMutex); qcomChains.erase(chain); }
static void qcomApply(QcomChain& c) { // qcomMutex held; successful wait, before application drawing.
    auto& s=qcomSessions.at(c.session);
    bool report=(c.frames++%300)==0;
    if(c.acquired.empty() || c.acquired.front()>=c.images.size()) return;
    if(eglGetCurrentContext()==EGL_NO_CONTEXT || eglGetCurrentContext()!=s.context) {
        if(report) LOG("QCOM skipped: expected GLES context not current on image-wait thread");
        return;
    }
    if(!s.glChecked) {
        GLint n=0; glGetIntegerv(GL_NUM_EXTENSIONS,&n);
        for(GLint i=0;i<n;++i) { auto e=glGetStringi(GL_EXTENSIONS,i); if(e && !strcmp(reinterpret_cast<const char*>(e),"GL_QCOM_texture_foveated")) s.glSupported=true; }
        s.glChecked=true;
    }
    using SetFoveation=void(*)(GLuint,GLuint,GLuint,GLfloat,GLfloat,GLfloat,GLfloat,GLfloat);
    auto set=reinterpret_cast<SetFoveation>(eglGetProcAddress("glTextureFoveationParametersQCOM"));
    if(!s.glSupported || !set) { if(report) LOG("QCOM unavailable in application context"); return; }
    // This opt-in diagnostic consumes GL errors; report preexisting errors rather than attributing them to our calls.
    GLenum prior=glGetError();
    if(prior!=GL_NO_ERROR) { if(report) LOG("QCOM skipped: preexisting GL error=0x%x",prior); return; }
    GLint binding=0; glGetIntegerv(GL_TEXTURE_BINDING_2D_ARRAY,&binding);
    GLuint texture=c.images[c.acquired.front()].image;
    glBindTexture(GL_TEXTURE_2D_ARRAY,texture);
    // This runtime exposes ordinary array images even for MSAA requests (OVR
    // multisampled-render-to-texture). Never reinterpret a real multisample target.
    GLenum bindError=glGetError();
    if(bindError!=GL_NO_ERROR) {
        glBindTexture(GL_TEXTURE_2D_ARRAY,binding);
        if(report) LOG("QCOM incompatible array target: samples=%u error=0x%x",c.requestedSamples,bindError);
        return;
    }
    using GetLevel=void(*)(GLenum,GLint,GLenum,GLint*);
    auto getLevel=reinterpret_cast<GetLevel>(eglGetProcAddress("glGetTexLevelParameteriv"));
    GLint width=0,height=0,depth=0;
    if(getLevel) {
        getLevel(GL_TEXTURE_2D_ARRAY,0,0x1000,&width);
        getLevel(GL_TEXTURE_2D_ARRAY,0,0x1001,&height);
        getLevel(GL_TEXTURE_2D_ARRAY,0,0x8071,&depth);
    }
    GLenum levelError=glGetError();
    if(!getLevel || levelError!=GL_NO_ERROR || width!=static_cast<GLint>(c.width) || height!=static_cast<GLint>(c.height) || depth!=2) {
        glBindTexture(GL_TEXTURE_2D_ARRAY,binding);
        if(report) LOG("QCOM incompatible storage: samples=%u size=%dx%dx%d error=0x%x",c.requestedSamples,width,height,depth,levelError);
        return;
    }
    if(report) LOG("QCOM array storage verified: requestedSamples=%u target=2D_ARRAY size=%dx%dx%d",c.requestedSamples,width,height,depth);
    constexpr GLenum features=0x8BFB,query=0x8BFD,points=0x8BFE,density=0x8BFC;
    GLint available=0,enabled=0, focalCount=0;
    glGetTexParameteriv(GL_TEXTURE_2D_ARRAY,query,&available);
    glGetTexParameteriv(GL_TEXTURE_2D_ARRAY,features,&enabled);
    glGetTexParameteriv(GL_TEXTURE_2D_ARRAY,points,&focalCount);
    GLenum error=glGetError();
    if(runtimeEyeFoveation()) {
        using GetFloat=void(*)(GLenum,GLenum,GLfloat*);
        auto getFloat=reinterpret_cast<GetFloat>(eglGetProcAddress("glGetTexParameterfv"));
        GLfloat floor=-1;
        if(getFloat) getFloat(GL_TEXTURE_2D_ARRAY,density,&floor);
        auto queryError=glGetError();
        bool fresh=c.mapped && s.running && s.time>0 && std::chrono::steady_clock::now()-s.sampled<std::chrono::milliseconds(100);
        bool diagnostic=runtimeEyeDiagnostic();
        bool coarse=fresh && (diagnostic?runtimeDiagnosticCoarse():s.focalValid);
        float gain=diagnostic?runtimeDiagnosticGain():qcomSetting("qcomGain",24.f,0.f,32.f);
        float area=diagnostic?0.f:qcomSetting("qcomArea",.15f,0.f,2.f);
        float requestedFloor=diagnostic?.125f:qcomSetting("qcomDensity",.35f,.125f,1.f);
        bool changed=c.diagnosticLast!=int(coarse);
        if(report || changed) LOG("Gaze fallback detail: full=%d fresh=%d focalValid=%d running=%d render=%d ageMs=%lld gazeResult=%d mode=%d eyes=%d/%d views=%u flags=%llu diagnostic=%d hold=%d",!coarse,fresh,s.focalValid,s.running,s.time>0,(long long)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-s.sampled).count(),s.gazeResult,s.gazeMode,s.eyeStates[0],s.eyeStates[1],s.viewCount,(unsigned long long)s.viewFlags,diagnostic,s.holding);
        if(report || changed) LOG("Runtime-eye GPU before texture=%u enabled=%d available=%d points=%d minDensity=%.3f error=0x%x mapped=%d",texture,enabled,available,focalCount,floor,error?error:queryError,c.mapped);
        if(error==GL_NO_ERROR && queryError==GL_NO_ERROR && getFloat && (available&3)==3 && focalCount>0 && focalCount<=16 && (fresh || (enabled&1))) {
            if(!(enabled&1)) glTexParameteri(GL_TEXTURE_2D_ARRAY,features,3);
            glTexParameterf(GL_TEXTURE_2D_ARRAY,density,coarse?requestedFloor:1.f);
            for(GLuint eye=0;eye<2;++eye) for(GLint point=0;point<focalCount;++point)
                set(texture,eye,point,diagnostic?c.diagnosticX[eye]:s.focalX[eye],diagnostic?c.diagnosticY[eye]:s.focalY[eye],coarse?gain:0.f,coarse?gain:0.f,coarse?area:1.f);
            auto writeError=glGetError();
            if(writeError!=GL_NO_ERROR) glTexParameterf(GL_TEXTURE_2D_ARRAY,density,1.f);
            getFloat(GL_TEXTURE_2D_ARRAY,density,&floor);
            if(report || changed) LOG("Runtime-eye GPU applied phase=%s texture=%u allPoints=%d layers=2 minDensity=%.3f centers=(%.3f,%.3f)/(%.3f,%.3f) gain=%.2f area=%.2f error=0x%x",coarse?(diagnostic?"COARSE":"GAZE"):"FULL",texture,focalCount,floor,diagnostic?c.diagnosticX[0]:s.focalX[0],diagnostic?c.diagnosticY[0]:s.focalY[0],diagnostic?c.diagnosticX[1]:s.focalX[1],diagnostic?c.diagnosticY[1]:s.focalY[1],coarse?gain:0.f,coarse?area:1.f,writeError);
            c.diagnosticLast=int(coarse);
        }
        glBindTexture(GL_TEXTURE_2D_ARRAY,binding);
        return; // Runtime profile writes remain enabled and share the same phase.
    }
    if(error!=GL_NO_ERROR || (available&3)!=3 || focalCount<1 || focalCount>16) {
        glBindTexture(GL_TEXTURE_2D_ARRAY,binding);
        if(report) LOG("QCOM unsupported texture features=%d points=%d error=0x%x",available,focalCount,error);
        return;
    }
    float x[2]{},y[2]{};
    bool fresh=c.mapped && s.running && s.time>0 &&
        std::chrono::steady_clock::now()-s.sampled<std::chrono::milliseconds(100);
    bool diagnostic=qcomDiagnostic();
    int phase=static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now()-s.diagnosticStarted).count()/10)%2;
    bool valid=fresh && s.tracker && s.space && s.getEyes;
    XrResult gazeResult=XR_ERROR_TIME_INVALID;
    int gazeMode=-1;
    if(valid && !diagnostic) {
        XrEyesANDROID eyes{XR_TYPE_EYES_ANDROID};
        XrEyesGetInfoANDROID get{XR_TYPE_EYES_GET_INFO_ANDROID,nullptr,s.time,s.space};
        gazeResult=s.getEyes(s.tracker,&get,&eyes);
        gazeMode=eyes.mode;
        XrView views[2]{{XR_TYPE_VIEW},{XR_TYPE_VIEW}};
        XrViewState state{XR_TYPE_VIEW_STATE}; uint32_t count=0;
        XrViewLocateInfo locate{XR_TYPE_VIEW_LOCATE_INFO,nullptr,XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,s.time,s.space};
        auto locateViews=symbol<PFN_xrLocateViews>("xrLocateViews");
        valid=gazeResult==XR_SUCCESS && eyes.mode==XR_EYE_TRACKING_MODE_BOTH_ANDROID && locateViews &&
            locateViews(c.session,&locate,&state,2,&count,views)==XR_SUCCESS && count==2 &&
            (state.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT);
        for(int i=0; valid && i<2; ++i) valid=eyes.eyes[i].eyeState==XR_EYE_STATE_GAZING_ANDROID && qcomProject(eyes.eyes[i].eyePose.orientation,views[i],x[i],y[i]);
    }
    if(diagnostic) {
        valid=fresh; // Deliberately independent of eye movement and tracking validity.
        x[0]=x[1]=-.65f; y[0]=y[1]=0.f;
    }
    if(!valid && !(enabled&1)) {
        glBindTexture(GL_TEXTURE_2D_ARRAY,binding);
        if(report) LOG("QCOM waiting: mapped=%d gazeResult=%d mode=%d; texture unchanged",c.mapped,gazeResult,gazeMode);
        return;
    }
    if(!(enabled&1)) glTexParameteri(GL_TEXTURE_2D_ARRAY,features,3);
    // Broad fovea; floor 0.5 to avoid an aggressive first hardware test. Full density on any invalidity.
    bool reduced=valid && (!diagnostic || phase==1);
    float floor=diagnostic?.125f:qcomSetting("qcomDensity",.35f,.125f,1.f);
    float gain=diagnostic?8.f:qcomSetting("qcomGain",24.f,0.f,32.f);
    float area=diagnostic?.05f:qcomSetting("qcomArea",.15f,0.f,2.f);
    glTexParameterf(GL_TEXTURE_2D_ARRAY,density,reduced?floor:1.f);
    for(GLuint eye=0;eye<2;++eye) for(GLint point=0;point<focalCount;++point)
        set(texture,eye,point,valid?x[eye]:0.f,valid?y[eye]:0.f,reduced?gain:0.f,reduced?gain:0.f,area);
    error=glGetError();
    if(error!=GL_NO_ERROR) { glTexParameterf(GL_TEXTURE_2D_ARRAY,density,1.f); }
    glBindTexture(GL_TEXTURE_2D_ARRAY,binding);
    if(diagnostic && (report || c.diagnosticLast!=phase)) {
        LOG("QCOM DIAGNOSTIC phase=%s fresh=%d floor=%.3f focal=(-0.65,0) gain=8 area=0.05 error=0x%x; alternates every 10s",phase?"FIXED_LEFT":"FULL",valid,reduced?floor:1.f,error);
        c.diagnosticLast=phase;
    }
    if(report && !diagnostic) LOG("QCOM quality floor=%.3f gain=%.3f area=%.3f",floor,gain,area);
    if(report) LOG("QCOM frame mapped=%d gazeResult=%d valid=%d error=0x%x image=%u (API acceptance is not GPU savings)",c.mapped,gazeResult,valid,error,texture);
}
#else
static void qcomSessionCreated(XrSession,const XrSessionCreateInfo*) {}
static void qcomRunning(XrSession,bool) {}
static void qcomForget(XrSession) {}
static void qcomFrame(XrSession,const XrFrameState*) {}
static void qcomChainCreated(XrSession,const XrSwapchainCreateInfo*,XrSwapchain) {}
static bool qcomOwns(XrSwapchain) { return false; }
static void qcomChainDestroyed(XrSwapchain) {}
#endif

extern "C" XRAPI_ATTR XrResult XRAPI_CALL xrAcquireSwapchainImage(XrSwapchain chain,const XrSwapchainImageAcquireInfo* info,uint32_t* index) {
    auto fn=symbol<PFN_xrAcquireSwapchainImage>("xrAcquireSwapchainImage");
    auto result=fn?fn(chain,info,index):XR_ERROR_FUNCTION_UNSUPPORTED;
    if(result==XR_SUCCESS && index) eyeAcquire(chain,*index);
#if defined(__ANDROID__) || defined(GXR_QCOM_TEST)
    if(result==XR_SUCCESS && index) { std::lock_guard<std::mutex> lock(qcomMutex); auto it=qcomChains.find(chain); if(it!=qcomChains.end()) it->second.acquired.push_back(*index); }
#endif
    return result;
}
extern "C" XRAPI_ATTR XrResult XRAPI_CALL xrWaitSwapchainImage(XrSwapchain chain,const XrSwapchainImageWaitInfo* info) {
    eyePrepareWait(chain);
    auto fn=symbol<PFN_xrWaitSwapchainImage>("xrWaitSwapchainImage");
    auto result=fn?fn(chain,info):XR_ERROR_FUNCTION_UNSUPPORTED;
#if defined(__ANDROID__) || defined(GXR_QCOM_TEST)
    if(result==XR_SUCCESS) { std::lock_guard<std::mutex> lock(qcomMutex); auto it=qcomChains.find(chain); if(it!=qcomChains.end()) qcomApply(it->second); }
#endif
    return result;
}
extern "C" XRAPI_ATTR XrResult XRAPI_CALL xrReleaseSwapchainImage(XrSwapchain chain,const XrSwapchainImageReleaseInfo* info) {
    auto fn=symbol<PFN_xrReleaseSwapchainImage>("xrReleaseSwapchainImage");
    auto result=fn?fn(chain,info):XR_ERROR_FUNCTION_UNSUPPORTED;
    if(result==XR_SUCCESS) eyeRelease(chain);
#if defined(__ANDROID__) || defined(GXR_QCOM_TEST)
    if(result==XR_SUCCESS) { std::lock_guard<std::mutex> lock(qcomMutex); auto it=qcomChains.find(chain); if(it!=qcomChains.end() && !it->second.acquired.empty()) it->second.acquired.pop_front(); }
#endif
    return result;
}
static void qcomOnEndFrame(XrSession session,const XrFrameEndInfo* info,XrResult result) {
#if defined(__ANDROID__) || defined(GXR_QCOM_TEST)
    if(result==XR_SUCCESS && info) {
        std::lock_guard<std::mutex> lock(qcomMutex);
        for(auto& pair:qcomChains) if(pair.second.session==session) pair.second.mapped=false;
        for(uint32_t i=0;i<info->layerCount;++i) {
            auto base=info->layers[i]; if(!base || base->type!=XR_TYPE_COMPOSITION_LAYER_PROJECTION) continue;
            auto layer=reinterpret_cast<const XrCompositionLayerProjection*>(base);
            if(layer->viewCount!=2) continue;
            auto chain=layer->views[0].subImage.swapchain;
            auto it=qcomChains.find(chain); if(it==qcomChains.end() || it->second.session!=session) continue;
            bool full=true;
            for(uint32_t eye=0;eye<2;++eye) { const auto& sub=layer->views[eye].subImage;
                full=full && sub.swapchain==chain && sub.imageArrayIndex==eye && sub.imageRect.offset.x==0 && sub.imageRect.offset.y==0 &&
                    sub.imageRect.extent.width==static_cast<int32_t>(it->second.width) && sub.imageRect.extent.height==static_cast<int32_t>(it->second.height);
            }
            it->second.mapped=full;
            if(full && runtimeEyeDiagnostic()) for(uint32_t eye=0;eye<2;++eye)
                it->second.mapped=runtimeDiagnosticCenter(layer->views[eye].fov,it->second.diagnosticX[eye],it->second.diagnosticY[eye]) && it->second.mapped;
        }
    }
#else
    (void)session; (void)info; (void)result;
#endif
}
