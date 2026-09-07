// BrazilMR — logging nativo. Android → logcat; host (testes) → stderr.
#pragma once

#if defined(__ANDROID__)
#include <android/log.h>
#define BMR_LOG_TAG "BrazilMR"
#define BMR_LOGI(...) __android_log_print(ANDROID_LOG_INFO,  BMR_LOG_TAG, __VA_ARGS__)
#define BMR_LOGW(...) __android_log_print(ANDROID_LOG_WARN,  BMR_LOG_TAG, __VA_ARGS__)
#define BMR_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, BMR_LOG_TAG, __VA_ARGS__)
#define BMR_LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, BMR_LOG_TAG, __VA_ARGS__)
#else
#include <cstdio>
#define BMR_LOGI(...) do { std::fprintf(stderr, "[I][BrazilMR] " __VA_ARGS__); std::fprintf(stderr, "\n"); } while (0)
#define BMR_LOGW(...) do { std::fprintf(stderr, "[W][BrazilMR] " __VA_ARGS__); std::fprintf(stderr, "\n"); } while (0)
#define BMR_LOGE(...) do { std::fprintf(stderr, "[E][BrazilMR] " __VA_ARGS__); std::fprintf(stderr, "\n"); } while (0)
#define BMR_LOGD(...) do { std::fprintf(stderr, "[D][BrazilMR] " __VA_ARGS__); std::fprintf(stderr, "\n"); } while (0)
#endif
