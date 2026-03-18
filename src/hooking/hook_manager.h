#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <string>
#include <mutex>

namespace texthook {

class HookManager {
public:
    bool init(const std::string& dataPath);
    void shutdown();

    bool injectIntoProcess(DWORD pid);
    void detachFromProcess();
    bool isInjected() const;

    std::string getLatestText();
    std::string lastError() const;

    void setMinTextLength(int len);
    void setDeduplication(bool enabled);
    void setHookFilter(const std::string& filter);

private:
    // DLL injection via CreateRemoteThread + LoadLibraryW
    bool injectDll(DWORD pid, const std::wstring& dllPath);
    bool ejectDll(DWORD pid, const std::wstring& dllPath);

    // Named pipe server — receives text from hook DLL
    void startPipeServer();
    void stopPipeServer();
    void pipeThreadFunc();

    // Resolve hook DLL path next to this DLL
    std::wstring resolveHookDllPath() const;

    DWORD m_targetPid = 0;
    HANDLE m_pipeThread = nullptr;
    volatile bool m_running = false;
    std::string m_dataPath;
    std::string m_error;

    // Text buffer with deduplication
    std::mutex m_textMutex;
    std::string m_latestText;

    // Ring buffer for deduplication — last 10 strings
    static constexpr int DEDUP_RING_SIZE = 10;
    std::string m_dedupRing[DEDUP_RING_SIZE];
    int m_dedupIndex = 0;

    int m_minTextLen = 2;
    bool m_dedup = true;
    std::string m_hookFilter = "all"; // "all", "textout", "drawtext", "glyph"
};

} // namespace texthook
