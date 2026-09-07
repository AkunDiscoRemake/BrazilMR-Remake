// ===========================================================================
// BrazilMR — handler nativo de crash (SIGSEGV/SIGBUS/SIGABRT/SIGFPE/SIGILL).
//
// O app ainda não rodou em aparelhos Android 14/15/16: quando o processo
// morre com um sinal (ex.: falha no driver de GPU novíssimo), o Java não
// captura nada. Este handler grava um mini-tombstone (sinal, endereço da
// falha e backtrace) no diretório de crashes do app ANTES de o processo
// morrer. Na abertura seguinte o CrashReporter (Kotlin) oferece o
// relatório para compartilhar.
//
// Best-effort: usa apenas open/write/close; re-entrega o sinal ao
// comportamento padrão para o sistema gerar o tombstone oficial também.
// ===========================================================================
#include <jni.h>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <unwind.h>

namespace {

char g_crashDir[512] = {0};

const char* signalName(int sig) {
    switch (sig) {
        case SIGSEGV: return "SIGSEGV (acesso invalido de memoria)";
        case SIGBUS:  return "SIGBUS (acesso invalido de barramento)";
        case SIGABRT: return "SIGABRT (abort - ex.: excecao C++ nao capturada)";
        case SIGFPE:  return "SIGFPE (erro aritmetico)";
        case SIGILL:  return "SIGILL (instrucao ilegal)";
        default:      return "sinal desconhecido";
    }
}

struct BacktraceCtx {
    int fd;
    int frames;
};

_Unwind_Reason_Code unwindCallback(struct _Unwind_Context* ctx, void* data) {
    auto* bt = static_cast<BacktraceCtx*>(data);
    if (bt->frames >= 24) return _URC_END_OF_STACK;
    uintptr_t ip = reinterpret_cast<uintptr_t>(_Unwind_GetIP(ctx));
    char line[64];
    snprintf(line, sizeof(line), "  #%02d pc %p\n",
             bt->frames++, reinterpret_cast<void*>(ip));
    write(bt->fd, line, strlen(line));
    return _URC_NO_REASON;
}

void crashHandler(int sig, siginfo_t* info, void*) {
    char path[576];
    snprintf(path, sizeof(path), "%s/native-crash-%d.txt",
             g_crashDir, static_cast<int>(getpid()));
    int fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0600);
    if (fd >= 0) {
        char head[384];
        snprintf(head, sizeof(head),
                 "sinal=%d (%s)\ncodigo=%d endereco=%p tid=%d pid=%d\n"
                 "obs=crash nativo no processo do app (motor ou lib de sistema)\n\n"
                 "backtrace (enderecos; simbolique com llvm-symbolizer + libbrazilmr.so com simbolos):\n",
                 sig, signalName(sig),
                 info ? info->si_code : 0,
                 info ? info->si_addr : nullptr,
                 static_cast<int>(gettid()), static_cast<int>(getpid()));
        write(fd, head, strlen(head));
        BacktraceCtx bt{fd, 0};
        _Unwind_Backtrace(unwindCallback, &bt);
        close(fd);
    }
    // devolve ao padrão → tombstone do sistema + processo encerrado
    signal(sig, SIG_DFL);
    raise(sig);
}

} // namespace

extern "C" {

// Chamado pelo CrashReporter (Kotlin) no com.brazilmr.BrazilMrApplication,
// o mais cedo possível no ciclo de vida do processo.
JNIEXPORT jboolean JNICALL
Java_com_brazilmr_sdk_NativeSdk_installCrashHandler(JNIEnv* env, jobject,
                                                    jstring crashDir) {
    if (!crashDir) return JNI_FALSE;
    const char* d = env->GetStringUTFChars(crashDir, nullptr);
    if (!d) return JNI_FALSE;
    snprintf(g_crashDir, sizeof(g_crashDir), "%s", d);
    env->ReleaseStringUTFChars(crashDir, d);
    if (g_crashDir[0] == '\0') return JNI_FALSE;

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = crashHandler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);

    bool ok = true;
    ok &= sigaction(SIGSEGV, &sa, nullptr) == 0;
    ok &= sigaction(SIGBUS,  &sa, nullptr) == 0;
    ok &= sigaction(SIGABRT, &sa, nullptr) == 0;
    ok &= sigaction(SIGFPE,  &sa, nullptr) == 0;
    ok &= sigaction(SIGILL,  &sa, nullptr) == 0;
    return ok ? JNI_TRUE : JNI_FALSE;
}

} // extern "C"
