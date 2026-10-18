#define GXR_POLICY_TEST
#define GXR_RUNTIME_EYE_TEST
#include "policy.cpp"
#include <cassert>
static void* testSymbol(const char*) { return nullptr; }
int main(int argc, char** argv) {
    if (argc != 1 && argc != 3) {
        fprintf(stderr, "Usage: %s [libopenxr_android.so libfoveation_manager.so]\n", argv[0]);
        return 2;
    }
    float focal[]{0,0,6,7,2};
    testConfig="foveation=runtime-eye\nqcomGain=32\nqcomArea=0\n";
    runtime_eye::applyFocal(true,.3f,-.4f,focal);
    assert(focal[0]==.3f && focal[1]==-.4f && focal[2]==32 && focal[3]==32 && focal[4]==0);
    runtime_eye::applyFocal(false,0,0,focal);
    assert(focal[0]==0 && focal[1]==0 && focal[2]==0 && focal[3]==0 && focal[4]==1);
    assert(gxr_runtime_hash::sha256("")=="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    assert(gxr_runtime_hash::sha256("abc")=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    // Proprietary device dumps are optional inputs, never CI dependencies.
    if (argc == 3) {
        assert(runtime_eye::hash(argv[1],"a7dabaa1030cc7979efe987f212488c61b61e8afb9aa3add59f7ae4fbeff4c92"));
        assert(runtime_eye::hash(argv[2],"90cab49ed1d737fd7cf234cc1d5c8af8f6ab107f2543d5543bc186aef8191255"));
        puts("PASS: supplied runtime library hashes");
    }
    std::vector<uint64_t> session(0x900/8),gl(2),holder(0x300/8),h(0x300/8),manager(2);
    auto ptr=[](auto& x){return reinterpret_cast<uintptr_t>(x.data());};
    session[0]=0x007373657372786fULL; session[0x810/8]=1; session[0x840/8]=ptr(gl);
    session[0x838/8]=ptr(holder);gl[0]=ptr(holder);holder[0x248/8]=ptr(h);h[0x200/8]=ptr(manager);
    auto m=runtime_eye::maps();
    assert(runtime_eye::handler(m,ptr(session))==ptr(h));
    assert(runtime_eye::handler(m,ptr(session)|0xb400000000000000ULL)==ptr(h));
    holder[0x248/8]=ptr(h)|0xb400000000000000ULL;
    assert(runtime_eye::handler(m,ptr(session))==(ptr(h)|0xb400000000000000ULL));
    holder[0x248/8]=ptr(h);
    session[0]=0;assert(!runtime_eye::handler(m,ptr(session)));session[0]=0x007373657372786fULL;
    session[0x810/8]=2;assert(!runtime_eye::handler(m,ptr(session)));session[0x810/8]=1;
    session[0x838/8]=2;assert(!runtime_eye::handler(m,ptr(session)));
    assert(!runtime_eye::range(m,UINTPTR_MAX-3,8));
    testConfig="foveation=runtime-eye\n";assert(eyeFoveation() && !qcomFoveation());
    puts("Runtime-eye SHA256 vectors, valid chain, wrong handle/state/pointer, overflow and mode isolation passed");
}
