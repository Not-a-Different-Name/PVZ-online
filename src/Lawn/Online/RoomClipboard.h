#pragma once
#include <string>
#include <cstdint>
#include <cstring>
#ifdef __ANDROID__
#include <SDL.h>
#include <jni.h>
#else
#include <windows.h>
#endif

namespace RoomClipboard {
constexpr std::size_t Limit = 4096;
enum class Kind { Read = 0, Copy = 1, Share = 2 };
enum class State { Idle, Pending, Success, Failed };
#ifndef __ANDROID__
inline bool Write(const std::string& text) {
    if (text.empty() || text.size() > Limit || text.find('\0') != std::string::npos) return false;
    const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), int(text.size()), nullptr, 0);
    if (count <= 0) return false;
    std::wstring wide(std::size_t(count), L'\0');
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), int(text.size()), &wide[0], count)) return false;
    const auto window = CreateWindowExW(0, L"STATIC", L"PVZ room clipboard", 0, 0, 0, 1, 1, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!window) return false;
    if (!OpenClipboard(window)) { DestroyWindow(window); return false; }
    const auto handle = GlobalAlloc(GMEM_MOVEABLE, (wide.size() + 1) * sizeof(wchar_t));
    bool success = false;
    if (handle) {
        auto* memory = GlobalLock(handle);
        if (memory) {
            memcpy(memory, wide.c_str(), (wide.size() + 1) * sizeof(wchar_t)); GlobalUnlock(handle);
            success = EmptyClipboard() && SetClipboardData(CF_UNICODETEXT, handle);
        }
        if (!success) GlobalFree(handle);
    }
    CloseClipboard(); DestroyWindow(window); return success;
}
inline bool Read(std::string& text) {
    if (!OpenClipboard(nullptr)) return false;
    const auto handle = GetClipboardData(CF_UNICODETEXT);
    const auto* memory = handle ? static_cast<const wchar_t*>(GlobalLock(handle)) : nullptr;
    bool success = false;
    if (memory) {
        const auto capacity = GlobalSize(handle) / sizeof(wchar_t);
        std::size_t length = 0;
        while (length < capacity && length <= Limit && memory[length]) ++length;
        if (length && length < capacity && length <= Limit) {
            const auto count = WideCharToMultiByte(CP_UTF8, 0, memory, int(length), nullptr, 0, nullptr, nullptr);
            if (count > 0 && count <= int(Limit)) {
                std::string result(std::size_t(count), '\0');
                if (WideCharToMultiByte(CP_UTF8, 0, memory, int(length), &result[0], count, nullptr, nullptr)) { text = result; success = true; }
            }
        }
        GlobalUnlock(handle);
    }
    CloseClipboard(); return success;
}
#endif
class Job {
public:
    State state = State::Idle;
    Kind kind = Kind::Read;
    std::string text;
    Job() = default;
    Job(const Job&) = delete;
    Job& operator=(const Job&) = delete;
    ~Job() { Cancel(); }
    bool Start(Kind action, const std::string& value = {}) {
        if (state == State::Pending || value.size() > Limit || value.find('\0') != std::string::npos) return false;
        Cancel(); kind = action; text.clear(); state = State::Pending;
#ifdef __ANDROID__
        static int serial = 0; serial = serial == 0x7ffffffe ? 1 : serial + 1; request = serial;
        const bool sent = WithActivity([&](JNIEnv* env, jobject activity, jclass type) {
            const auto method = env->GetMethodID(type, "requestRoomClipboard", "(IILjava/lang/String;)V");
            const auto input = env->NewStringUTF(value.c_str());
            if (method && input) env->CallVoidMethod(activity, method, jint(request), jint(action), input);
            if (input) env->DeleteLocalRef(input);
            return method != nullptr && input != nullptr;
        });
        if (!sent) state = State::Failed;
        began = SDL_GetTicks();
#else
        const bool success = action == Kind::Read ? Read(text) : Write(value);
        state = success ? State::Success : State::Failed;
#endif
        return true;
    }
    void Poll() {
#ifdef __ANDROID__
        if (state != State::Pending) return;
        const bool success = WithActivity([&](JNIEnv* env, jobject activity, jclass type) {
            const auto method = env->GetMethodID(type, "roomClipboardStatus", "(I)I");
            if (!method) return false;
            const auto result = env->CallIntMethod(activity, method, jint(request));
            if (result == 1) return true;
            if (result != 2) { state = State::Failed; return true; }
            if (kind == Kind::Read) {
                const auto take = env->GetMethodID(type, "takeRoomClipboardText", "(I)Ljava/lang/String;");
                if (!take) return false;
                auto value = static_cast<jstring>(env->CallObjectMethod(activity, take, jint(request)));
                if (!value) return false;
                const auto count = env->GetStringUTFLength(value);
                const auto* bytes = count > 0 && count <= int(Limit) ? env->GetStringUTFChars(value, nullptr) : nullptr;
                if (bytes) { text.assign(bytes, std::size_t(count)); env->ReleaseStringUTFChars(value, bytes); }
                env->DeleteLocalRef(value);
                if (text.empty()) return false;
            }
            state = State::Success; return true;
        });
        if (!success || (state == State::Pending && Uint32(SDL_GetTicks() - began) > 5000)) state = State::Failed;
        if (state != State::Pending) Finish();
#endif
    }
    void Cancel() {
#ifdef __ANDROID__
        Finish();
#endif
        state = State::Idle; text.clear();
    }
private:
#ifdef __ANDROID__
    int request = 0;
    Uint32 began = 0;
    template<typename Callback> bool WithActivity(Callback callback) {
        auto* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
        if (!env) return false;
        auto activity = static_cast<jobject>(SDL_AndroidGetActivity());
        if (!activity) return false;
        const auto type = env->GetObjectClass(activity);
        bool result = type && callback(env, activity, type);
        if (env->ExceptionCheck()) { env->ExceptionClear(); result = false; }
        if (type) env->DeleteLocalRef(type);
        env->DeleteLocalRef(activity); return result;
    }
    void Finish() {
        if (!request) return;
        WithActivity([&](JNIEnv* env, jobject activity, jclass type) {
            const auto method = env->GetMethodID(type, "finishRoomClipboard", "(I)V");
            if (method) env->CallVoidMethod(activity, method, jint(request));
            return method != nullptr;
        });
        request = 0;
    }
#endif
};
}
