#pragma once
#include <jni.h>
#include <unistd.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <elf.h>
#include <fcntl.h>

// Obfuscation Macro for Security Strings
#ifndef O
#define O(x) x // Fallback if oxorany not included, but we'll use it in main
#endif

namespace Security {

    // 1. Anti-Debugger (ptrace)
    inline void CheckDebugger() {
        if (ptrace(PTRACE_TRACEME, 0, 1, 0) < 0) {
            // Debugger detected!
            exit(0);
        }
        ptrace(PTRACE_DETACH, 0, 1, 0);
    }

    // 2. Anti-Frida / Anti-Hooking (Scanning Maps)
    inline void CheckFrida() {
        char line[512];
        FILE* fp = fopen("/proc/self/maps", "r");
        if (fp) {
            while (fgets(line, sizeof(line), fp)) {
                if (strstr(line, "frida") || strstr(line, "gum-js") || strstr(line, "linjector")) {
                    fclose(fp);
                    exit(0);
                }
            }
            fclose(fp);
        }
    }

    // 3. Root Detection (Common Binaries)
    inline void CheckRoot() {
        const char* paths[] = { "/system/app/Superuser.apk", "/sbin/su", "/system/bin/su", "/system/xbin/su", "/data/local/xbin/su", "/data/local/bin/su", "/system/sd/xbin/su", "/system/bin/failsafe/su", "/data/local/su" };
        for (int i = 0; i < 9; i++) { if (access(paths[i], F_OK) == 0) exit(0); }
    }

    // 4. Anti-Emulator (Checking for QEMU/Genymotion)
    inline void CheckEmulator() {
        const char* emuPaths[] = { "/system/lib/libc_malloc_debug_qemu.so", "/sys/qemu_trace", "/system/bin/qemu-props" };
        for (int i = 0; i < 3; i++) { if (access(emuPaths[i], F_OK) == 0) exit(0); }
    }

    // 5. Anti-Virtual Space (Checking for common virtual environments)
    inline void CheckVirtualSpace() {
        char line[512];
        FILE* fp = fopen("/proc/self/maps", "r");
        if (fp) {
            while (fgets(line, sizeof(line), fp)) {
                if (strstr(line, "io.va.exposed") || strstr(line, "com.lody.virtual") || strstr(line, "com.applisto.appcloner")) {
                    fclose(fp);
                    exit(0);
                }
            }
            fclose(fp);
        }
    }

    // 6. Frida Port Check
    inline void CheckFridaPort() {
        struct sockaddr_in sa;
        memset(&sa, 0, sizeof(sa));
        sa.sin_family = AF_INET;
        sa.sin_port = htons(27042); // Default Frida Port
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (connect(sock, (struct sockaddr*)&sa, sizeof(sa)) == 0) {
            close(sock);
            exit(0);
        }
        close(sock);
    }

    // 7. In-Memory Integrity Check (Anti-Patch)
    inline uint32_t CalculateChecksum(uintptr_t base, size_t size) {
        uint32_t checksum = 0;
        uint8_t* ptr = (uint8_t*)base;
        for (size_t i = 0; i < size; i++) {
            checksum = (checksum << 5) + checksum + ptr[i];
        }
        return checksum;
    }

    static uintptr_t libBase = 0;
    static size_t libSize = 0;
    static uint32_t initialChecksum = 0;
    static bool securityThreadActive = false;
    static uint64_t securityHeartbeat = 0; // Heartbeat counter

    // 9. Xposed Framework Detection (JNI)
    inline void CheckXposed(JNIEnv* env) {
        jclass xposed = env->FindClass(O("de/robv/android/xposed/XposedBridge"));
        if (xposed != nullptr) exit(0);
        env->ExceptionClear();
    }

    // 11. Package Name Integrity Check (Anti-Tamper)
    inline void CheckPackageName(JNIEnv* env, const char* expectedPackage) {
        jclass activityThreadClass = env->FindClass(O("android/app/ActivityThread"));
        if (!activityThreadClass) {
            env->ExceptionClear();
            return;
        }
        jmethodID currentPackageNameMethod = env->GetStaticMethodID(activityThreadClass, O("currentPackageName"), O("()Ljava/lang/String;"));
        if (!currentPackageNameMethod) {
            env->ExceptionClear();
            return;
        }
        jstring packageNameStr = (jstring)env->CallStaticObjectMethod(activityThreadClass, currentPackageNameMethod);
        if (packageNameStr) {
            const char* packageNameChars = env->GetStringUTFChars(packageNameStr, nullptr);
            if (packageNameChars) {
                if (strcmp(packageNameChars, expectedPackage) != 0) {
                    // Package name mismatch (tampered/injected) - Exit immediately
                    exit(0);
                }
                env->ReleaseStringUTFChars(packageNameStr, packageNameChars);
            }
        }
        env->ExceptionClear();
    }

    // 10. Background Security Thread (Paranoid)
    static void* SecurityThread(void*) {
        securityThreadActive = true;
        while (true) {
            securityHeartbeat++; // Increment heartbeat
            
            // CheckDebugger();
            // CheckFrida();
            // CheckFridaPort();
            CheckVirtualSpace();
            
            // Verify code integrity
            /*
            if (libBase != 0 && initialChecksum != 0) {
                if (CalculateChecksum(libBase, libSize) != initialChecksum) {
                    exit(0); // Code patched!
                }
            }
            */
            
            sleep(5); // Increased check frequency
        }
        return nullptr;
    }

    inline void StartProtection(uintptr_t base, size_t size) {
        libBase = base;
        libSize = size;
        if (base != 0) initialChecksum = CalculateChecksum(base, size);
        
        // Prevent Memory Dumping
        prctl(PR_SET_DUMPABLE, 0);
        
        // Temporarily disabled for debugging
        // CheckDebugger();
        // CheckEmulator();
        // CheckRoot();
        CheckVirtualSpace();
        
        pthread_t t;
        pthread_create(&t, nullptr, SecurityThread, nullptr);
        pthread_detach(t);
    }
}
