#include "JavaFunc.h"
#include <jni.h>
#include <dlfcn.h>
#include <cstdio>
#include <cstdlib>
#include <sys/mman.h>
#include <unistd.h>
#include <pthread.h>

#include "ELF/elf_util.h"
#include "classes.h"

#define ANDROID_RUNTIME_DSO "libandroid_runtime.so"

typedef struct JniInvocation {
    const char* jni_provider_library_name;
    void* jni_provider_library;

    jint (*JNI_GetDefaultJavaVMInitArgs)(void*);

    jint (*JNI_CreateJavaVM)(JavaVM**, JNIEnv**, void*);

    jint (*JNI_GetCreatedJavaVMs)(JavaVM**, jsize, jsize*);
} JniInvocationImpl;


typedef struct JavaContext {
    JavaVM* vm;
    JNIEnv* env;
    JniInvocationImpl* invoc;
} JavaCTX;

static JavaCTX ctx;
static jclass entryClass = nullptr;

static void modify_function(void* func) {
    unsigned long page_start = (unsigned long)func & ~(4096 - 1);
    if (mprotect((void*)page_start, 4096, PROT_READ | PROT_WRITE | PROT_EXEC) == -1) {
        return;
    }
    unsigned char* p = (unsigned char*)func;
    //RET
    p[0] = 0xC0;
    p[1] = 0x03;
    p[2] = 0x5F;
    p[3] = 0xD6;
}

static jclass LoadJavaClassAndCallMain(JNIEnv* env, const char* class_name, void* bytes, size_t len) {
    auto clClass = env->FindClass("java/lang/ClassLoader");
    auto getSystemClassLoader = env->GetStaticMethodID(clClass, "getSystemClassLoader",
                                                       "()Ljava/lang/ClassLoader;");
    auto systemClassLoader = env->CallStaticObjectMethod(clClass, getSystemClassLoader);
    //LOGD("systemClassLoader %p", systemClassLoader);
    // Assuming we have a valid mapped module, load it. This is similar to the approach used for
    // Dynamite modules in GmsCompat, except we can use InMemoryDexClassLoader directly instead of
    // tampering with DelegateLastClassLoader's DexPathList.
    //   LOGD("create buffer");
    auto buf = env->NewDirectByteBuffer(bytes, len);
    //  LOGD("create class loader");
    auto dexClClass = env->FindClass("dalvik/system/InMemoryDexClassLoader");
    auto dexClInit = env->GetMethodID(dexClClass, "<init>",
                                      "(Ljava/nio/ByteBuffer;Ljava/lang/ClassLoader;)V");
    auto dexCl = env->NewObject(dexClClass, dexClInit, buf, systemClassLoader);

    // Load the class
    //  LOGD("load class");
    auto loadClass = env->GetMethodID(clClass, "loadClass",
                                      "(Ljava/lang/String;)Ljava/lang/Class;");
    auto entryClassName = env->NewStringUTF(class_name);
    //  LOGD("entryClassName %p", entryClassName);
    auto entryClassObj = env->CallObjectMethod(dexCl, loadClass, entryClassName);
    // LOGD("entryClassObj %p", entryClassObj);

    //call main
    entryClass = (jclass)entryClassObj;
    auto entryInit = env->GetStaticMethodID(entryClass, "main", "([Ljava/lang/String;)V");
    env->CallStaticVoidMethod(entryClass, entryInit, nullptr);

    return entryClassObj == nullptr ? nullptr : (jclass)env->NewGlobalRef(entryClassObj);
}

static jclass LoadJavaClass(JNIEnv* env, const char* class_name, void* bytes, size_t len) {
    auto clClass = env->FindClass("java/lang/ClassLoader");
    auto getSystemClassLoader = env->GetStaticMethodID(clClass, "getSystemClassLoader",
                                                       "()Ljava/lang/ClassLoader;");
    auto systemClassLoader = env->CallStaticObjectMethod(clClass, getSystemClassLoader);
    // LOGD("systemClassLoader %p", systemClassLoader);
    // Assuming we have a valid mapped module, load it. This is similar to the approach used for
    // Dynamite modules in GmsCompat, except we can use InMemoryDexClassLoader directly instead of
    // tampering with DelegateLastClassLoader's DexPathList.
    // LOGD("create buffer");
    auto buf = env->NewDirectByteBuffer(bytes, len);
    //   LOGD("create class loader");
    auto dexClClass = env->FindClass("dalvik/system/InMemoryDexClassLoader");
    auto dexClInit = env->GetMethodID(dexClClass, "<init>",
                                      "(Ljava/nio/ByteBuffer;Ljava/lang/ClassLoader;)V");
    auto dexCl = env->NewObject(dexClClass, dexClInit, buf, systemClassLoader);

    // Load the class
    //   LOGD("load class");
    auto loadClass = env->GetMethodID(clClass, "loadClass",
                                      "(Ljava/lang/String;)Ljava/lang/Class;");
    auto entryClassName = env->NewStringUTF(class_name);
    // LOGD("entryClassName %p", entryClassName);
    auto entryClassObj = env->CallObjectMethod(dexCl, loadClass, entryClassName);
    //  LOGD("entryClassObj %p", entryClassObj);

    //call main
    entryClass = (jclass)entryClassObj;
    auto entryInit = env->GetStaticMethodID(entryClass, "main2", "([Ljava/lang/String;)V");
    env->CallStaticVoidMethod(entryClass, entryInit, nullptr);

    return entryClassObj == nullptr ? nullptr : (jclass)env->NewGlobalRef(entryClassObj);
}


static void JavaEnvDestructor(void*) {
    ctx.vm->DetachCurrentThread();
}

JNIEnv* JavaFunc::GetJavaEnv() {
    static uint32_t TlsSlot = 0;
    if (TlsSlot == 0) {
        pthread_key_create((pthread_key_t*)&TlsSlot, &JavaEnvDestructor);
    }
    auto* Env = (JNIEnv*)pthread_getspecific(TlsSlot);
    if (Env == nullptr) {
        ctx.vm->GetEnv((void**)&Env, JNI_VERSION_1_6);
        jint AttachResult = ctx.vm->AttachCurrentThread(&Env, nullptr);
        if (AttachResult == JNI_ERR) {
            return nullptr;
        }
        pthread_setspecific(TlsSlot, (void*)Env);
    }
    return Env;
}

int JavaFunc::init(char** jvm_options, uint8_t jvm_nb_options) {
    typedef jint (*JNI_CreateJavaVM_t)(JavaVM** p_vm, JNIEnv** p_env, void* vm_args);
    typedef void (*JniInvocation_ctor_t)(void*);
    typedef void (*JniInvocation_dtor_t)(void*);
    typedef void (*JniInvocation_Init_t)(void*, const char*);

    JNI_CreateJavaVM_t JNI_CreateJVM;
    JniInvocationImpl* (*JniInvocationCreate)();
    bool (*JniInvocationInit)(JniInvocationImpl*, const char*);
    jint (*registerFrameworkNatives)(JNIEnv*);
    int (*startReg)(JNIEnv*);
    void* runtime_dso;

    JniInvocation_ctor_t JniInvocation_ctor;
    //JniInvocation_dtor_t JniInvocation_dtor;
    JniInvocation_Init_t JniInvocation_Init;

    if ((runtime_dso = dlopen(ANDROID_RUNTIME_DSO, RTLD_NOW)) == NULL) {
        printf("[!] %s\n", dlerror());
        return -1;
    }

    JNI_CreateJVM = (JNI_CreateJavaVM_t)dlsym(runtime_dso, "JNI_CreateJavaVM");
    if (!JNI_CreateJVM) {
        printf("Cannot find JNI_CreateJavaVM\n");
        return -1;
    }

    registerFrameworkNatives = (jint (*)(JNIEnv*))dlsym(runtime_dso, "registerFrameworkNatives");
    if (!registerFrameworkNatives) {
        registerFrameworkNatives = (jint (*)(JNIEnv*))dlsym(runtime_dso,
                                                            "Java_com_android_internal_util_WithFramework_registerNatives");
        if (!registerFrameworkNatives) {
            printf("Cannot find registerFrameworkNatives\n");
            return -1;
        }
    }
    startReg = (int (*)(JNIEnv*))dlsym(runtime_dso, "_ZN7android14AndroidRuntime8startRegEP7_JNIEnv");


    JniInvocationCreate = (JniInvocationImpl *(*)())dlsym(runtime_dso, "JniInvocationCreate");
    JniInvocationInit = (bool (*)(JniInvocationImpl*, const char*))dlsym(runtime_dso, "JniInvocationInit");
    if (JniInvocationCreate && JniInvocationInit) {
        ctx.invoc = JniInvocationCreate();
        JniInvocationInit(ctx.invoc, ANDROID_RUNTIME_DSO);
    } else {
        JniInvocation_ctor = (JniInvocation_ctor_t)dlsym(runtime_dso, "_ZN13JniInvocationC1Ev");
        JniInvocation_Init = (JniInvocation_Init_t)dlsym(runtime_dso, "_ZN13JniInvocation4InitEPKc");

        if (!JniInvocation_ctor || !JniInvocation_Init) {
            printf("Cannot find JniInvocationImpl\n");
            return -1;
        }

        ctx.invoc = (JniInvocationImpl*)calloc(1, 256);
        JniInvocation_ctor(ctx.invoc);
        JniInvocation_Init(ctx.invoc, NULL);
    }

    JavaVMOption options[jvm_nb_options];
    JavaVMInitArgs args;
    args.version = JNI_VERSION_1_6;
    if (jvm_nb_options > 0) {
        for (int i = 0; i < jvm_nb_options; ++i)
            options[i].optionString = jvm_options[i];

        args.nOptions = jvm_nb_options;
        args.options = options;
    } else {
        args.nOptions = 0;
        args.options = NULL;
    }

    args.ignoreUnrecognized = JNI_TRUE;

    int api_level = android_get_device_api_level();
    if (api_level < 31) {
        const char* symbols[] = {
            "InitializeSignalChain",
            "ClaimSignalChain",
            "UnclaimSignalChain",
            "InvokeUserSignalHandler",
            "EnsureFrontOfChain",
            "AddSpecialSignalHandlerFn",
            "RemoveSpecialSignalHandlerFn",
            NULL
        };
        ElfImg elf("libsigchain.so");
        if (elf.isValid()) {
            for (const char** sym = symbols; *sym; ++sym) {
                auto func = elf.getSymbAddress(*sym);

                if (!func) {
                    continue;
                }
                modify_function((void*)func);
            }
        }
    }


    jint status = JNI_CreateJVM(&ctx.vm, &ctx.env, &args);
    if (status == JNI_ERR) return -1;

    printf("[d] vm: %p, env: %p\n", ctx.vm, ctx.env);

    void* vm = dlsym(runtime_dso, "_ZN7android14AndroidRuntime7mJavaVME");
    *(void**)vm = ctx.vm;

    startReg(ctx.env);

    entryClass = LoadJavaClassAndCallMain(GetJavaEnv(), "com.example.mylibrary.Main", classes, sizeof(classes));

    if (entryClass == nullptr) {
        printf("entryClass is null");
        return -1;
    }

    return 0;
}

int JavaFunc::init(JavaVM* vm) {
    ctx.vm = vm;
    entryClass = LoadJavaClass(GetJavaEnv(), "com.example.mylibrary.Main", classes, sizeof(classes));
    if (entryClass == nullptr) {
        printf("entryClass is null");
        return -1;
    }
    return 0;
}


JavaVM* JavaFunc::GetJavaVM() {
    return ctx.vm;
}

void JavaFunc::injectTouchEvent(TouchAction action, int pointerId, int x, int y) {
    auto env = GetJavaEnv();
    if (env == nullptr) {
        return;
    }
    static jmethodID pJmethodId = nullptr;
    if (pJmethodId == nullptr) {
        pJmethodId = env->GetStaticMethodID(entryClass, "injectTouchEvent",
                                            "(IJII)V");
    }

    env->CallStaticVoidMethod(entryClass, pJmethodId, action, (long)pointerId, x, y);
}

//public static View getView(int width, int height, boolean hide, boolean secure)
jobject JavaFunc::getView(int width, int height, bool hide, bool secure) {
    auto env = GetJavaEnv();
    if (env == nullptr) {
        return nullptr;
    }
    static jmethodID pJmethodId = nullptr;
    if (pJmethodId == nullptr) {
        pJmethodId = env->GetStaticMethodID(entryClass, "getView",
                                            "(IIZZ)Landroid/view/View;");
    }
    auto obj = env->CallStaticObjectMethod(entryClass, pJmethodId, width, height, hide, secure);
    return obj ? env->NewGlobalRef(obj) : nullptr;
}

jobject JavaFunc::checkView() {
    // public static View view = null;
    auto env = GetJavaEnv();
    if (env == nullptr) {
        return nullptr;
    }
    static jfieldID pJfieldId = nullptr;
    if (pJfieldId == nullptr) {
        pJfieldId = env->GetStaticFieldID(entryClass, "view", "Landroid/view/View;");
    }
    auto obj = env->GetStaticObjectField(entryClass, pJfieldId);
    return obj ? env->NewGlobalRef(obj) : nullptr;
}

//public static Surface getSurface(View view)
jobject JavaFunc::getSurface(jobject view) {
    auto env = GetJavaEnv();
    if (env == nullptr) {
        return nullptr;
    }
    static jmethodID pJmethodId = nullptr;
    if (pJmethodId == nullptr) {
        pJmethodId = env->GetStaticMethodID(entryClass, "getSurface",
                                            "(Landroid/view/View;)Landroid/view/Surface;");
    }
    auto obj = env->CallStaticObjectMethod(entryClass, pJmethodId, view);
    return obj ? env->NewGlobalRef(obj) : nullptr;
}

//    public static void removeView(View view)
void JavaFunc::removeView(jobject view) {
    auto env = GetJavaEnv();
    if (env == nullptr) {
        return;
    }
    static jmethodID pJmethodId = nullptr;
    if (pJmethodId == nullptr) {
        pJmethodId = env->GetStaticMethodID(entryClass, "removeView",
                                            "(Landroid/view/View;)V");
    }
    env->CallStaticVoidMethod(entryClass, pJmethodId, view);
}

//  public static int[] getDisplayInfo()
JavaFunc::DisplayInfo JavaFunc::getDisplayInfo() {
    auto env = GetJavaEnv();
    if (env == nullptr) {
        return {};
    }
    static jmethodID pJmethodId = nullptr;
    if (pJmethodId == nullptr) {
        pJmethodId = env->GetStaticMethodID(entryClass, "getDisplayInfo",
                                            "()[I");
    }
    auto displayInfo = env->CallStaticObjectMethod(entryClass, pJmethodId);
    jintArray displayInfoArray = (jintArray)displayInfo;
    jint* displayInfoData = env->GetIntArrayElements(displayInfoArray, nullptr);
    DisplayInfo info = {displayInfoData[0], displayInfoData[1], displayInfoData[2]};
    env->ReleaseIntArrayElements(displayInfoArray, displayInfoData, 0);
    return info;
}

//public static String getClipboardText()
std::string JavaFunc::getClipboardText() {
    auto env = GetJavaEnv();
    if (env == nullptr) {
        return {};
    }
    static jmethodID pJmethodId = nullptr;
    if (pJmethodId == nullptr) {
        pJmethodId = env->GetStaticMethodID(entryClass, "getClipboardText",
                                            "()Ljava/lang/String;");
    }
    printf( "getClipboardText %p\n", pJmethodId);
    seteuid(2000);
    auto clipboardText = (jstring)env->CallStaticObjectMethod(entryClass, pJmethodId);
    seteuid(0);
    const char* clipboardTextChars = env->GetStringUTFChars(clipboardText, nullptr);
    std::string clipboardTextStr = clipboardTextChars;
    env->ReleaseStringUTFChars(clipboardText, clipboardTextChars);

    return clipboardTextStr;
}

//public static boolean setClipboardText(String text)
bool JavaFunc::setClipboardText(const std::string& text) {
    auto env = GetJavaEnv();
    if (env == nullptr) {
        return false;
    }
    static jmethodID pJmethodId = nullptr;
    if (pJmethodId == nullptr) {
        pJmethodId = env->GetStaticMethodID(entryClass, "setClipboardText",
                                            "(Ljava/lang/String;)V");
    }
    auto textObj = env->NewStringUTF(text.c_str());
    seteuid(2000);
    auto ret= env->CallStaticBooleanMethod(entryClass, pJmethodId, textObj);
    seteuid(0);
    return ret;
}

void JavaFunc::loop() {
    auto env = GetJavaEnv();
    if (env == nullptr) {
        return;
    }
    static jmethodID pJmethodId = nullptr;
    if (pJmethodId == nullptr) {
        pJmethodId = env->GetStaticMethodID(entryClass, "loop", "()V");
    }
    env->CallStaticVoidMethod(entryClass, pJmethodId);
}

// public static Surface createNativeWindow(int width, int height, boolean isHide, boolean isSecure)
jobject JavaFunc::createNativeWindow(int width, int height, bool hide, bool secure) {
    auto env = GetJavaEnv();
    if (env == nullptr) {
        return nullptr;
    }
    static jmethodID pJmethodId = nullptr;
    if (pJmethodId == nullptr) {
        pJmethodId = env->GetStaticMethodID(entryClass, "createNativeWindow",
                                            "(IIZZ)Landroid/view/Surface;");
    }
    auto obj = env->CallStaticObjectMethod(entryClass, pJmethodId, width, height, hide, secure);
    return obj ? env->NewGlobalRef(obj) : nullptr;
}

//public static void destroyNativeWindow(Surface surface)
void JavaFunc::destroyNativeWindow(jobject surface) {
    auto env = GetJavaEnv();
    if (env == nullptr) {
        return;
    }
    static jmethodID pJmethodId = nullptr;
    if (pJmethodId == nullptr) {
        pJmethodId = env->GetStaticMethodID(entryClass, "destroyNativeWindow",
                                            "(Landroid/view/Surface;)V");
    }
    env->CallStaticVoidMethod(entryClass, pJmethodId, surface);
}


extern "C" {
JNIEXPORT void InitializeSignalChain() {
}

JNIEXPORT void ClaimSignalChain() {
}

JNIEXPORT void UnclaimSignalChain() {
}

JNIEXPORT void InvokeUserSignalHandler() {
}

JNIEXPORT void EnsureFrontOfChain() {
}

JNIEXPORT void AddSpecialSignalHandlerFn() {
}

JNIEXPORT void RemoveSpecialSignalHandlerFn() {
}
}
