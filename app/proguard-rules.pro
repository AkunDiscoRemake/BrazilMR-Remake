# BrazilMR ProGuard/R8

# ARCore
-keep class com.google.ar.core.** { *; }
-keepclassmembers class com.google.ar.core.** { *; }

# MediaPipe Tasks
-keep class com.google.mediapipe.** { *; }
-dontwarn com.google.mediapipe.**
-keep class com.google.protobuf.** { *; }
-dontwarn com.google.protobuf.**

# LuaJ — usa reflexão para corrotinas/standard libs
-keep class org.luaj.** { *; }
-dontwarn org.luaj.**

# JNI: pontes nativas do BrazilMR
-keepclasseswithmembernames class com.brazilmr.** {
    native <methods>;
}
-keep class com.brazilmr.sdk.NativeSdk { *; }
-keep class com.brazilmr.sdk.NativeEvents { *; }

# Acessibilidade / serviços
-keep class com.brazilmr.apps.bridge.VrAccessibilityService { *; }
