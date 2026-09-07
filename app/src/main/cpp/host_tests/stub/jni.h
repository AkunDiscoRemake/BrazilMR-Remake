// HOST-ONLY — stub de <jni.h> para verificação de sintaxe da ponte JNI
// fora do Android (não linka). No device, o jni.h do NDK é usado.
#ifndef BMR_HOST_JNI_STUB
#define BMR_HOST_JNI_STUB

#include <cstdint>

typedef uint8_t  jboolean;
typedef int8_t   jbyte;
typedef int32_t  jint;
typedef int64_t  jlong;
typedef float    jfloat;
typedef double   jdouble;
typedef void*    jobject;
typedef jobject  jstring;
typedef jobject  jarray;
typedef jarray   jfloatArray;
typedef jarray   jbyteArray;
typedef jarray   jintArray;
typedef jint     jsize;

#define JNI_ABORT 2
#define JNI_TRUE  1
#define JNI_FALSE 0
#define JNIEXPORT
#define JNICALL

struct JNIEnv {
    const char* GetStringUTFChars(jstring, jboolean* isCopy) { (void)isCopy; return ""; }
    void ReleaseStringUTFChars(jstring, const char*) {}
    jsize GetArrayLength(jarray) { return 0; }
    void GetFloatArrayRegion(jfloatArray, jsize, jsize, jfloat*) {}
    void SetFloatArrayRegion(jfloatArray, jsize, jsize, const jfloat*) {}
    void GetIntArrayRegion(jintArray, jsize, jsize, jint*) {}
    void SetIntArrayRegion(jintArray, jsize, jsize, const jint*) {}
    jfloatArray NewFloatArray(jsize) { return nullptr; }
    jstring NewStringUTF(const char*) { return nullptr; }
    jbyte* GetByteArrayElements(jbyteArray, jboolean* isCopy) { (void)isCopy; return nullptr; }
    void ReleaseByteArrayElements(jbyteArray, jbyte*, jint) {}
    void* GetDirectBufferAddress(jobject) { return nullptr; }
};

#endif // BMR_HOST_JNI_STUB
