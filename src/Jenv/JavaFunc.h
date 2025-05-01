#pragma once
#include <cstdint>
#include <jni.h>
#include <string>

namespace JavaFunc {
    enum TouchAction {
        DOWN = 0,
        UP = 1,
        MOVE = 2
    };

    struct DisplayInfo {
        int width;
        int height;
        int orientation;
    };


    int init(char** jvm_options = nullptr, uint8_t jvm_nb_options = 0);

    int init(JavaVM* vm);

    JNIEnv* GetJavaEnv();

    JavaVM* GetJavaVM();

    void injectTouchEvent(TouchAction action, int pointerId, int x, int y);

    jobject getView(int width, int height, bool hide, bool secure);

    jobject checkView();

    jobject getSurface(jobject view);

    void removeView(jobject view);

    DisplayInfo getDisplayInfo();

    std::string getClipboardText();

    bool setClipboardText(const std::string& text);

    void loop();

    jobject createNativeWindow(int width, int height, bool hide, bool secure);

    void destroyNativeWindow(jobject surface);
}
