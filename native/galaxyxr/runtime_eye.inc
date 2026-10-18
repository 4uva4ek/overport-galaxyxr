// App-local, version-pinned runtime backend. No system writes or global properties.
#if defined(__ANDROID__) || defined(GXR_RUNTIME_EYE_TEST)
#include "runtime_eye_sha256.h"
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <sys/uio.h>
#include <sys/syscall.h>
#include <limits>
namespace runtime_eye {
// Pinned manager ABI: 23 virtual slots at model+0x33018.
// Keep the copy alive until process exit (including manager destruction).
static uintptr_t eyeTable[25]{};
static std::mutex tableMutex;
static std::unordered_map<void*,XrSession> managerSessions;
using CalculateGL = int(*)(void*, const void*, const void*, const void*, const void*, float*);
static CalculateGL originalGL = nullptr;
static void applyFocal(bool valid,float x,float y,float* output) {
    if(valid) {
        output[0]=x; output[1]=y;
        // Share the patch-time gain/area controls with the final GLES bridge.
        // The acquired-image path also applies these values to every focal slot.
        output[2]=output[3]=qcomSetting("qcomGain",28.f,0.f,32.f);
        output[4]=qcomSetting("qcomArea",.5f,0.f,2.f);
    }
    else { output[0]=output[1]=output[2]=output[3]=0; output[4]=1; }
}
static int eyeGL(void* self,const void* fov,const void* pose,const void* config,const void* extra,float* output) {
    int result=originalGL(self,fov,pose,config,extra,output);
    bool valid=false; float x=0,y=0; uint32_t eye=2;
    std::memcpy(&eye,config,sizeof(eye));
#if defined(__ANDROID__)
    XrSession session=XR_NULL_HANDLE;
    { std::lock_guard<std::mutex> lock(tableMutex); auto it=managerSessions.find(self); if(it!=managerSessions.end()) session=it->second; }
    {
        std::lock_guard<std::mutex> lock(qcomMutex);
        auto it=qcomSessions.find(session);
        if(eye<2 && it!=qcomSessions.end()) {
            auto& s=it->second;
            valid=s.running && s.time>0 && s.focalValid && std::chrono::steady_clock::now()-s.sampled<std::chrono::milliseconds(100);
            x=s.focalX[eye]; y=s.focalY[eye];
        }
    }
#endif
    if(result==0) applyFocal(valid,x,y,output);
    if(result==0 && runtimeEyeDiagnostic()) {
        bool reduced=runtimeDiagnosticCoarse();
        XrFovf viewFov{}; std::memcpy(&viewFov,fov,sizeof(viewFov));
        if(!runtimeDiagnosticCenter(viewFov,output[0],output[1])) { reduced=false; output[0]=output[1]=0; }
        output[2]=output[3]=reduced?runtimeDiagnosticGain():0.f; output[4]=reduced?0.f:1.f;
        static int last=-1;
        int phase=runtimeDiagnosticPhase();
        if(__atomic_exchange_n(&last,phase,__ATOMIC_RELAXED)!=phase) { LOG("Runtime-eye DIAGNOSTIC %s: independent of gaze",phase==0?"FULL":phase==1?"FIXED_CENTER":"MOVING_CENTER"); }
    }
    static unsigned count=0;
    if((__atomic_fetch_add(&count,1u,__ATOMIC_RELAXED)%900)==0 && result==0)
        LOG("Runtime-eye GL eye=%u gazeValid=%d focal=(%.4f,%.4f) gain=(%.3f,%.3f) area=%.3f",eye,valid,output[0],output[1],output[2],output[3],output[4]);
    return result;
}
struct Mapping { uintptr_t lo, hi, offset; bool read, write, execute; std::string path; };
static std::vector<Mapping> maps() {
    std::ifstream input("/proc/self/maps"); std::vector<Mapping> result; std::string line;
    while(std::getline(input,line)) {
        unsigned long lo=0,hi=0,offset=0; char perms[5]{}; int end=0;
        if(std::sscanf(line.c_str(),"%lx-%lx %4s %lx %*s %*s %n",&lo,&hi,perms,&offset,&end)!=4) continue;
        std::string path=line.substr(end); result.push_back({lo,hi,offset,perms[0]=='r',perms[1]=='w',perms[2]=='x',path});
    }
    return result;
}
// Android arm64 malloc uses top-byte-tagged pointers; maps and process_vm_readv
// use untagged virtual addresses. Preserve original pointers for the runtime call.
static uintptr_t address(uintptr_t p) { return p & 0x00ffffffffffffffULL; }
static const Mapping* range(const std::vector<Mapping>& m, uintptr_t p, size_t n) {
    if(!p || n>std::numeric_limits<uintptr_t>::max()-p) return nullptr;
    p=address(p);
    for(auto& r:m) if(r.read && p>=r.lo && p+n<=r.hi) return &r;
    return nullptr;
}
static bool object(const std::vector<Mapping>& m,uintptr_t p,size_t size) {
    auto r=range(m,p,size);
    return p%8==0 && r && r->write && !r->execute && (r->path.empty() || r->path=="[heap]" || r->path.find("[anon:")==0);
}
template<class T> static bool read(const std::vector<Mapping>& m,uintptr_t p,T& out) {
    if(!range(m,p,sizeof(T))) return false;
    iovec local{&out,sizeof(T)},remote{reinterpret_cast<void*>(address(p)),sizeof(T)};
    return syscall(__NR_process_vm_readv,getpid(),&local,1,&remote,1,0)==sizeof(T);
}
static uintptr_t base(const std::vector<Mapping>& m,const std::string& path) {
    for(auto& r:m) if(r.path==path && r.offset==0 && r.read && !r.write) return r.lo;
    return 0;
}
static bool hash(const std::string& path,const char* expected) {
    std::ifstream f(path,std::ios::binary|std::ios::ate); if(!f) return false;
    auto size=f.tellg(); if(size<=0 || size>64*1024*1024) return false;
    std::string data(static_cast<size_t>(size),'\0'); f.seekg(0);
    return bool(f.read(data.data(),size)) && gxr_runtime_hash::sha256(data)==expected;
}
// Magic/state exactly as checked by oxr_xrBeginSession in the pinned binary.
// Reject opaque loader wrappers instead of scanning arbitrary memory for a handle.
static uintptr_t handler(const std::vector<Mapping>& m,uintptr_t session) {
    uint64_t magic=0; uint32_t state=0; uintptr_t gl=0,native=0,holder=0,h=0,manager=0;
    if(!object(m,session,0x848)) { LOG("Runtime-eye guard: session memory"); return 0; }
    if(!read(m,session,magic) || magic!=0x007373657372786fULL) { LOG("Runtime-eye guard: session signature"); return 0; }
    if(!read(m,session+0x810,state) || state!=1) { LOG("Runtime-eye guard: session state"); return 0; }
    if(!read(m,session+0x840,gl) || !object(m,gl,8)) { LOG("Runtime-eye guard: GL compositor"); return 0; }
    // create_session_compositor_gl copies session+0x838 into GL compositor[0].
    // The +0x1cf0 hop in the handoff belongs to a native swapchain, NOT this object.
    if(!read(m,session+0x838,native) || !read(m,gl,holder) || address(native)!=address(holder) || !object(m,holder,0x280)) { LOG("Runtime-eye guard: native compositor cross-check"); return 0; }
    if(!read(m,holder+0x248,h) || !object(m,h,0x218)) { LOG("Runtime-eye guard: handler"); return 0; }
    if(!read(m,h+0x200,manager) || !object(m,manager,8)) { LOG("Runtime-eye guard: manager"); return 0; }
    uint8_t flag=0;
    if(!read(m,h+0x208,flag) || flag>1) { LOG("Runtime-eye guard: handler flag"); return 0; }
    return h;
}
static int enable(XrSession session) {
    const std::string runtime="/system/lib64/libopenxr_android.so", model="/system/lib64/libfoveation_manager.so";
    auto m=maps(); auto b=base(m,runtime);
    if(!b || !base(m,model)) { LOG("Runtime-eye refused: expected libraries not mapped"); return -1; }
    // Cache only immutable file identity; recheck mapped addresses and objects each session.
    static const bool verified=hash(runtime,"a7dabaa1030cc7979efe987f212488c61b61e8afb9aa3add59f7ae4fbeff4c92") &&
        hash(model,"90cab49ed1d737fd7cf234cc1d5c8af8f6ab107f2543d5543bc186aef8191255");
    if(!verified) { LOG("Runtime-eye refused: runtime SHA256 mismatch/unreadable"); return -1; }
    auto code=range(m,b+0x1cb208,16);
    if(!code || !code->execute || code->write || code->path!=runtime) { LOG("Runtime-eye refused: executable mapping mismatch"); return -1; }
    uint32_t entry=0;
    if(!read(m,b+0x1cb208,entry) || entry!=0xd503233f) { LOG("Runtime-eye refused: function entry mismatch"); return -1; }
    auto h=handler(m,reinterpret_cast<uintptr_t>(session));
    if(!h) { LOG("Runtime-eye refused: session magic/state or pointer chain invalid (no wrapper guessing)"); return -1; }
    uintptr_t manager=0,table=0,method=0;
    if(!read(m,h+0x200,manager) || !read(m,manager,table) || !read(m,table+0x18,method)) { LOG("Runtime-eye refused: model interface unreadable"); return -1; }
    auto vtable=range(m,address(table),0x28), target=range(m,address(method),4);
    if(!vtable || vtable->write || !target || !target->execute || target->write ||
       (target->path!=runtime && target->path!=model)) { LOG("Runtime-eye refused: model interface identity"); return -1; }
    LOG("Runtime-eye guards passed; enabling runtime model once for this GLES session");
    auto fn=reinterpret_cast<int(*)(void*)>(b+0x1cb208);
    int result=fn(reinterpret_cast<void*>(h)); uint8_t enabled=0;
    bool readable=read(m,h+0x208,enabled);
    LOG("Runtime-eye enable result=%d flag=%d; verify FoveationManager eye tracked model and image",result,readable?int(enabled):-1);
    if(result==0 && readable && enabled==1) {
        // Both GLES layers construct static configs. Override the final focal
        // result using independently sampled eyes, retaining runtime gains/area.
        // Other virtual methods and system code remain unchanged.
        auto mb=base(m,model);
        uintptr_t setter=0,calculate=0;
        if(table!=mb+0x33018 || !read(m,table+0x20,setter) || setter!=mb+0x13748 ||
           !read(m,table+0xb0,calculate) || calculate!=mb+0x15bcc ||
           !range(m,table-16,sizeof(eyeTable))) {
            LOG("Runtime-eye refused: pinned config vtable mismatch"); return -1;
        }
        std::lock_guard<std::mutex> lock(tableMutex);
        if(!originalGL) {
            for(size_t i=0;i<25;++i) if(!read(m,table-16+i*8,eyeTable[i])) return -1;
            originalGL=reinterpret_cast<CalculateGL>(calculate);
            eyeTable[2+22]=reinterpret_cast<uintptr_t>(&eyeGL);
        }
        managerSessions[reinterpret_cast<void*>(manager)]=session;
        // Installed before this session's first frame; aligned pointer publication.
        __atomic_store_n(reinterpret_cast<uintptr_t*>(manager),reinterpret_cast<uintptr_t>(eyeTable+2),__ATOMIC_RELEASE);
        LOG("Runtime-eye final GLES focal override installed: independent gaze, no competing texture writes");
    }
    return result;
}
}
#endif
static void enableRuntimeEye(XrSession session) {
    if(!runtimeEyeFoveation()) return;
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        auto it=sessions.find(session);
        if(it==sessions.end() || !it->second.policy.eye || !it->second.policy.gles || it->second.runtimeEyeAttempted) return;
        it->second.runtimeEyeAttempted=true;
    }
#if defined(__ANDROID__) || defined(GXR_RUNTIME_EYE_TEST)
    runtime_eye::enable(session);
#else
    (void)session;
#endif
}
