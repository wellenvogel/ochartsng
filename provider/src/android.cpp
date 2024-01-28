#include <jni.h>
#include <string>
#include <jni.h>
#include <jni.h>
#include <jni.h>

static bool running=false;
static bool doStop=false;
int mainFunction(int argc, char **argv, bool *stopFlag=NULL);

extern "C" JNIEXPORT jint JNICALL
Java_de_wellenvogel_ochartsprovider_LibHandler_runProvider(JNIEnv *env, jobject thiz,
                                                           jobjectArray stringArray) {

    if (running) return -888;
    doStop=false;
    int stringCount = env->GetArrayLength(stringArray);
    char *args[stringCount+1];
    for (int i=0; i<stringCount; i++) {
        jstring string = (jstring) (env->GetObjectArrayElement(stringArray, i));
        args[i] = (char *)env->GetStringUTFChars(string, 0);
        // Don't forget to call `ReleaseStringUTFChars` when you're done.
    }
    args[stringCount]=NULL;
    running=true;
    putenv("AVNAV_TEST_KEY=Decrypted");
    int rt=mainFunction(stringCount+1,args,&doStop);
    for (int i=0;i<stringCount;i++){
        jstring string = (jstring) (env->GetObjectArrayElement(stringArray, i));
        env->ReleaseStringUTFChars(string,args[i]);
    }
    running=false;
    return rt;
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_de_wellenvogel_ochartsprovider_LibHandler_isNativeRunning(JNIEnv *env, jobject thiz) {
           return running;
        }

extern "C"
JNIEXPORT void JNICALL
Java_de_wellenvogel_ochartsprovider_LibHandler_stopNative(JNIEnv* env,jobject thiz) {
    doStop=true;
}