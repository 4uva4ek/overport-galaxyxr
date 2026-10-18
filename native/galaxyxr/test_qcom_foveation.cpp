#define GXR_POLICY_TEST
#define GXR_QCOM_TEST
#include "policy.cpp"
#include <cassert>
#include <limits>
static EGLContext current=reinterpret_cast<void*>(1);
static GLuint bound=99;
static int enabled=0, writes=0;
static int pointCount=1;
static int storageWidth=1024;
static float densityValue=1, focalX[2]{}, focalY[2]{};
static bool tracking=true;
static XrResult waitResult=XR_SUCCESS;
static XrQuaternionf gazeOrientation{0,0,0,1};
static EGLContext eglGetCurrentContext(){return current;}
static void setFocal(GLuint,GLuint layer,GLuint,float x,float y,float,float,float){++writes;focalX[layer]=x;focalY[layer]=y;}
static void getLevel(GLenum,GLint,GLenum p,GLint* value){*value=p==0x8071?2:p==0x1000?storageWidth:1024;}
static void getFloat(GLenum,GLenum,GLfloat* value){*value=densityValue;}
static void (*eglGetProcAddress(const char* name))(){return !strcmp(name,"glGetTexParameterfv")?reinterpret_cast<void(*)()>(getFloat):!strcmp(name,"glGetTexLevelParameteriv")?reinterpret_cast<void(*)()>(getLevel):reinterpret_cast<void(*)()>(setFocal);}
static void glGetIntegerv(GLenum p,GLint* v){*v=p==GL_NUM_EXTENSIONS?1:bound;}
static const unsigned char* glGetStringi(GLenum,GLuint){return reinterpret_cast<const unsigned char*>("GL_QCOM_texture_foveated");}
static GLenum glGetError(){return 0;}
static void glBindTexture(GLenum,GLuint texture){bound=texture;}
static void glGetTexParameteriv(GLenum,GLenum p,GLint* v){*v=p==0x8BFD?3:p==0x8BFE?pointCount:enabled;}
static void glTexParameteri(GLenum,GLenum,GLint v){enabled=v;}
static void glTexParameterf(GLenum,GLenum,GLfloat v){densityValue=v;}
static XrResult eyes(XrEyeTrackerANDROID,const XrEyesGetInfoANDROID*,XrEyesANDROID* out){
    out->mode=tracking?XR_EYE_TRACKING_MODE_BOTH_ANDROID:XR_EYE_TRACKING_MODE_NOT_TRACKING_ANDROID;
    for(auto& eye:out->eyes){eye.eyeState=XR_EYE_STATE_GAZING_ANDROID;eye.eyePose.orientation=gazeOrientation;}
    return XR_SUCCESS;
}
static XrResult locate(XrSession,const XrViewLocateInfo*,XrViewState* state,uint32_t,uint32_t* count,XrView* views){
    *count=2;state->viewStateFlags=XR_VIEW_STATE_ORIENTATION_VALID_BIT;
    for(int i=0;i<2;++i){views[i].pose.orientation={0,0,0,1};views[i].fov={-.78539816f,.78539816f,.78539816f,-.78539816f};}
    return XR_SUCCESS;
}
static XrResult acquire(XrSwapchain,const XrSwapchainImageAcquireInfo*,uint32_t* index){*index=0;return XR_SUCCESS;}
static XrResult wait(XrSwapchain,const XrSwapchainImageWaitInfo*){return waitResult;}
static XrResult release(XrSwapchain,const XrSwapchainImageReleaseInfo*){return XR_SUCCESS;}
static int expectedLayers=-1, endCalls=0;
static XrResult endResult=XR_SUCCESS;
static XrResult endFrame(XrSession,const XrFrameEndInfo* info){
    ++endCalls;
    if(expectedLayers>=0) {
        assert(info && info->layerCount==static_cast<uint32_t>(expectedLayers));
        assert(info->layers[0]->type==XR_TYPE_COMPOSITION_LAYER_PROJECTION);
        if(expectedLayers==2) assert(info->layers[1]->type==XR_TYPE_COMPOSITION_LAYER_QUAD);
    }
    return endResult;
}
static XrResult waitFrame(XrSession,const XrFrameWaitInfo*,XrFrameState* frame){frame->shouldRender=XR_TRUE;frame->predictedDisplayTime=123;return XR_SUCCESS;}
static XrResult beginFrame(XrSession,const XrFrameBeginInfo*){return XR_SUCCESS;}
static XrResult createSurface(XrSession,const XrSwapchainCreateInfo*,XrSwapchain* chain,void** surface){*chain=reinterpret_cast<XrSwapchain>(90);*surface=reinterpret_cast<void*>(91);return XR_SUCCESS;}
static XrResult createSpace(XrSession,const XrReferenceSpaceCreateInfo*,XrSpace* space){*space=reinterpret_cast<XrSpace>(92);return XR_SUCCESS;}
static XrResult destroySpace(XrSpace){return XR_SUCCESS;}
static XrResult destroyChain(XrSwapchain){return XR_SUCCESS;}

static XrResult enumerateImages(XrSwapchain,uint32_t capacity,uint32_t* count,XrSwapchainImageBaseHeader* images){
    *count=1;if(capacity)reinterpret_cast<QcomImage*>(images)[0].image=7;return XR_SUCCESS;
}
static void* testSymbol(const char* n){
    if(!strcmp(n,"xrWaitFrame"))return reinterpret_cast<void*>(waitFrame);
    if(!strcmp(n,"xrBeginFrame"))return reinterpret_cast<void*>(beginFrame);
    if(!strcmp(n,"xrEnumerateSwapchainImages"))return reinterpret_cast<void*>(enumerateImages);
    if(!strcmp(n,"xrAcquireSwapchainImage"))return reinterpret_cast<void*>(acquire);
    if(!strcmp(n,"xrWaitSwapchainImage"))return reinterpret_cast<void*>(wait);
    if(!strcmp(n,"xrReleaseSwapchainImage"))return reinterpret_cast<void*>(release);
    if(!strcmp(n,"xrEndFrame"))return reinterpret_cast<void*>(endFrame);
    if(!strcmp(n,"xrLocateViews"))return reinterpret_cast<void*>(locate);
    return nullptr;
}
int main(){
    testConfig="foveation=qcom\n";
    testConfig="qcomDensity=nan\nqcomGain=99\nqcomArea=0.5junk\n";
    assert(qcomSetting("qcomDensity",.25f,.125f,1.f)==.25f);
    assert(qcomSetting("qcomGain",4.f,0.f,8.f)==4.f);
    assert(qcomSetting("qcomArea",.25f,0.f,2.f)==.25f);
    testConfig="foveation=qcom\nqcomDensity=0.375\n";
    assert(qcomSetting("qcomDensity",.25f,.125f,1.f)==.375f);
    testConfig="foveation=qcom\n";
    auto session=reinterpret_cast<XrSession>(1);auto chain=reinterpret_cast<XrSwapchain>(2);
    QcomSession s;s.context=current;s.tracker=reinterpret_cast<XrEyeTrackerANDROID>(3);s.space=reinterpret_cast<XrSpace>(4);s.getEyes=eyes;s.running=true;
    qcomSessions[session]=s;
    XrSwapchainCreateInfo ci{XR_TYPE_SWAPCHAIN_CREATE_INFO};ci.width=ci.height=1024;ci.arraySize=2;ci.faceCount=ci.mipCount=1;ci.sampleCount=4;ci.usageFlags=XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
    qcomChainCreated(session,&ci,chain);assert(qcomChains.empty());
    ci.sampleCount=2;qcomChainCreated(session,&ci,chain);assert(qcomChains.at(chain).requestedSamples==2);
    XrFrameState frame{XR_TYPE_FRAME_STATE};frame.predictedDisplayTime=123;frame.shouldRender=XR_TRUE;qcomFrame(session,&frame);
    uint32_t index=9;assert(xrAcquireSwapchainImage(chain,nullptr,&index)==XR_SUCCESS && index==0);
    waitResult=XR_TIMEOUT_EXPIRED;xrWaitSwapchainImage(chain,nullptr);assert(writes==0 && enabled==0);
    waitResult=XR_SUCCESS;xrWaitSwapchainImage(chain,nullptr);assert(writes==0 && enabled==0 && bound==99); // no verified eye mapping
    XrCompositionLayerProjectionView views[2]{{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW},{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW}};
    for(int i=0;i<2;++i)views[i].subImage={chain,{{0,0},{1024,1024}},static_cast<uint32_t>(i)};
    XrCompositionLayerProjection layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};layer.viewCount=2;layer.views=views;
    const XrCompositionLayerBaseHeader* layers[]={reinterpret_cast<const XrCompositionLayerBaseHeader*>(&layer)};
    XrFrameEndInfo end{XR_TYPE_FRAME_END_INFO};end.layerCount=1;end.layers=layers;xrEndFrame(session,&end);
    xrWaitSwapchainImage(chain,nullptr);assert(writes==2 && enabled==3 && densityValue==.5f && bound==99);
    assert(std::abs(focalX[0])<1e-5f && std::abs(focalY[1])<1e-5f);
    gazeOrientation={0,-std::sin(.15f),0,std::cos(.15f)};xrWaitSwapchainImage(chain,nullptr);assert(focalX[0]>.25f && focalX[1]>.25f);
    testConfig="foveation=qcom-debug\n";tracking=false;
    qcomSessions[session].diagnosticStarted=std::chrono::steady_clock::now();
    xrWaitSwapchainImage(chain,nullptr);assert(densityValue==1 && bound==99);
    qcomSessions[session].diagnosticStarted-=std::chrono::seconds(11);
    xrWaitSwapchainImage(chain,nullptr);assert(densityValue==.125f && focalX[0]==-.65f && focalX[1]==-.65f && focalY[0]==0);
    qcomSessions[session].sampled-=std::chrono::seconds(1);
    xrWaitSwapchainImage(chain,nullptr);assert(densityValue==1); // stale frame still fails closed
    qcomFrame(session,&frame);
    qcomSessions[session].diagnosticStarted-=std::chrono::seconds(10);
    xrWaitSwapchainImage(chain,nullptr);assert(densityValue==1); // second full-quality phase
    testConfig="foveation=qcom\n";tracking=true;
    testConfig="foveation=runtime-eye-debug\n";
    views[0].fov={-.950999f,.695963f,.917506f,-.917506f};
    views[1].fov={-.695963f,.950999f,.917506f,-.917506f};
    xrEndFrame(session,&end);
    pointCount=3;int allBefore=writes;
    runtimeDiagnosticStarted=std::chrono::steady_clock::now()-std::chrono::seconds(11);
    qcomFrame(session,&frame);xrWaitSwapchainImage(chain,nullptr);
    assert(writes==allBefore+6 && densityValue==.125f && bound==99 && !qcomOwns(chain));
    assert(focalX[0]>.25f && focalX[0]<.26f && focalX[1]<-.25f && focalX[1]>-.26f);
    runtimeDiagnosticStarted=std::chrono::steady_clock::now();xrWaitSwapchainImage(chain,nullptr);
    assert(densityValue==1 && bound==99);
    runtimeDiagnosticStarted=std::chrono::steady_clock::now()-std::chrono::seconds(21);
    float movingX=0,movingY=0;
    assert(runtimeDiagnosticPhase()==2 && runtimeDiagnosticCoarse());
    assert(runtimeDiagnosticCenter(views[0].fov,movingX,movingY) && movingX>.4f);
    runtimeDiagnosticStarted=std::chrono::steady_clock::now();
    testConfig="foveation=runtime-eye-fixed\n";
    runtimeDiagnosticStarted-=std::chrono::seconds(35);
    assert(runtimeDiagnosticPhase()==1 && runtimeDiagnosticCoarse());
    assert(runtimeDiagnosticGain()==32.f);
    xrEndFrame(session,&end);xrWaitSwapchainImage(chain,nullptr);
    assert(densityValue==.125f && focalX[0]>.25f && focalX[1]<-.25f);
    testConfig="foveation=runtime-eye\nqcomDensity=0.125\nqcomGain=8\nqcomArea=0\n";
    qcomSessions[session].focalValid=true;
    qcomSessions[session].focalX[0]=.2f;qcomSessions[session].focalX[1]=-.3f;
    allBefore=writes;xrWaitSwapchainImage(chain,nullptr);
    assert(writes==allBefore+6 && densityValue==.125f && focalX[0]==.2f && focalX[1]==-.3f && !qcomOwns(chain));
    qcomSessions[session].focalValid=false;xrWaitSwapchainImage(chain,nullptr);assert(densityValue==1);
    pointCount=1;testConfig="foveation=qcom\n";
    storageWidth=512;int storageBefore=writes;xrWaitSwapchainImage(chain,nullptr);assert(writes==storageBefore && bound==99);storageWidth=1024;
    tracking=false;xrWaitSwapchainImage(chain,nullptr);assert(densityValue==1 && bound==99);
    tracking=true;qcomSessions[session].sampled-=std::chrono::seconds(1);xrWaitSwapchainImage(chain,nullptr);assert(densityValue==1);
    qcomFrame(session,&frame);int before=writes;current=nullptr;xrWaitSwapchainImage(chain,nullptr);assert(writes==before);current=s.context;
    views[1].subImage.imageRect.offset.x=1;xrEndFrame(session,&end);xrWaitSwapchainImage(chain,nullptr);assert(densityValue==1 && !qcomChains.at(chain).mapped);
    xrReleaseSwapchainImage(chain,nullptr);assert(qcomChains.at(chain).acquired.empty());before=writes;xrWaitSwapchainImage(chain,nullptr);assert(writes==before);
    XrView v{XR_TYPE_VIEW};v.pose.orientation={0,0,0,1};v.fov={-.4f,.8f,.7f,-.5f};float x,y;
    assert(qcomProject({0,0,0,1},v,x,y) && x<0 && y<0); // asymmetric projection
    assert(!qcomProject({0,1,0,0},v,x,y)); // backwards gaze
    assert(!qcomProject({0,0,0,std::numeric_limits<float>::quiet_NaN()},v,x,y));
    // Integrated runtime-eye + HUD: public wait/begin/end hooks, real gaze
    // projection/mapping and GL application, with only runtime/Surface mocked.
    testConfig="foveation=runtime-eye\nfps_hud=on\nqcomDensity=0.5\nqcomGain=28\nqcomArea=0.5\n";
    views[1].subImage.imageRect.offset.x=0;
    tracking=true;pointCount=3;
    GxrFpsHud::Instance hud;
    hud.vm=reinterpret_cast<GxrFpsHud::Vm*>(1);hud.createSurface=createSurface;
    hud.createSpace=createSpace;hud.destroySpace=destroySpace;hud.destroySwapchain=destroyChain;
    auto instance=reinterpret_cast<XrInstance>(10);
    GxrFpsHud::instances[instance]=hud;
    gxrFpsRememberSession(instance,session,16);
    gxrFpsOnSessionState(session,XR_SESSION_STATE_FOCUSED);
    GxrFpsHud::testSurfaceAvailable=true;
    auto submit=[&](int count){
        assert(xrWaitFrame(session,nullptr,&frame)==XR_SUCCESS);
        assert(xrBeginFrame(session,nullptr)==XR_SUCCESS);
        expectedLayers=count;int previous=endCalls;
        assert(xrEndFrame(session,&end)==endResult && endCalls==previous+1);
    };
    submit(1);
    auto worker=GxrFpsHud::sessions.at(session)->worker;
    {std::unique_lock<std::mutex> lock(worker->mutex);
     assert(worker->changed.wait_for(lock,std::chrono::seconds(2),[&]{return worker->ready.load();}));}
    submit(2);
    assert(qcomChains.at(chain).mapped && qcomSessions.at(session).focalValid);
    xrAcquireSwapchainImage(chain,nullptr,&index);
    int integratedBefore=writes;xrWaitSwapchainImage(chain,nullptr);
    assert(writes==integratedBefore+6 && densityValue==.5f && bound==99);
    float previousX=focalX[0];gazeOrientation={0,std::sin(.15f),0,std::cos(.15f)};
    submit(2);xrWaitSwapchainImage(chain,nullptr);
    assert(std::abs(focalX[0]-previousX)>.1f);
    tracking=false;submit(2);xrWaitSwapchainImage(chain,nullptr);
    assert(densityValue==1 && !qcomSessions.at(session).focalValid);
    tracking=true;submit(2);xrWaitSwapchainImage(chain,nullptr);assert(densityValue==.5f);
    // The metrics exclude the diagnostic quad: one accepted interval / second.
    auto& metrics=GxrFpsHud::sessions.at(session)->metrics;
    metrics.reset();metrics.sample(gxrFpsNow()-1000000000ULL,true);
    submit(2);assert(metrics.valid() && metrics.fps()>.9 && metrics.fps()<1.1);
    endResult=XR_ERROR_RUNTIME_FAILURE;submit(2);
    assert(GxrFpsHud::sessions.at(session)->failed);
    endResult=XR_SUCCESS;submit(1);xrWaitSwapchainImage(chain,nullptr);
    assert(qcomChains.at(chain).mapped && densityValue==.5f);
    gxrFpsForgetInstance(instance);expectedLayers=-1;
    qcomForget(session);assert(qcomSessions.empty() && qcomChains.empty());
    puts("QCOM projection, mapping, timeout, context, invalid/stale gaze and lifecycle passed");
}
