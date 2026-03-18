#include "hook_manager.h"

#include <tlhelp32.h>
#include <algorithm>
#include <cstring>
#include <filesystem>

namespace texthook {

// Used as an address anchor for GetModuleHandleExW
static void thisModuleAnchor() {}

bool HookManager::init(const std::string& dataPath) {
    m_dataPath = dataPath;
    m_error.clear();
    return true;
}

void HookManager::shutdown() {
    detachFromProcess();
}

bool HookManager::injectIntoProcess(DWORD pid) {
    if (m_targetPid != 0) {
        m_error = "Already injected into PID " + std::to_string(m_targetPid);
        return false;
    }

    std::wstring hookPath = resolveHookDllPath();
    if (hookPath.empty()) {
        m_error = "Could not resolve hook DLL path";
        return false;
    }

    if (!std::filesystem::exists(hookPath)) {
        m_error = "Hook DLL not found";
        return false;
    }

    // Start pipe server before injection so the hook DLL can connect
    m_targetPid = pid;
    startPipeServer();

    if (!injectDll(pid, hookPath)) {
        stopPipeServer();
        m_targetPid = 0;
        return false;
    }

    return true;
}

void HookManager::detachFromProcess() {
    if (m_targetPid == 0) return;

    std::wstring hookPath = resolveHookDllPath();
    ejectDll(m_targetPid, hookPath);
    stopPipeServer();
    m_targetPid = 0;
}

bool HookManager::isInjected() const {
    return m_targetPid != 0;
}

std::string HookManager::getLatestText() {
    std::lock_guard<std::mutex> lock(m_textMutex);
    std::string result = m_latestText;
    m_latestText.clear();
    return result;
}

std::string HookManager::lastError() const {
    return m_error;
}

void HookManager::setMinTextLength(int len) {
    m_minTextLen = len;
}

void HookManager::setDeduplication(bool enabled) {
    m_dedup = enabled;
}

void HookManager::setHookFilter(const std::string& filter) {
    m_hookFilter = filter;
}

// --- Private ---

bool HookManager::injectDll(DWORD pid, const std::wstring& dllPath) {
    HANDLE hProcess = OpenProcess(
        PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
        PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
        FALSE, pid);

    if (!hProcess) {
        m_error = "OpenProcess failed (error " + std::to_string(GetLastError()) + ")";
        return false;
    }

    size_t pathBytes = (dllPath.size() + 1) * sizeof(wchar_t);
    void* remoteMemory = VirtualAllocEx(hProcess, nullptr, pathBytes,
                                         MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remoteMemory) {
        m_error = "VirtualAllocEx failed (error " + std::to_string(GetLastError()) + ")";
        CloseHandle(hProcess);
        return false;
    }

    if (!WriteProcessMemory(hProcess, remoteMemory, dllPath.c_str(), pathBytes, nullptr)) {
        m_error = "WriteProcessMemory failed (error " + std::to_string(GetLastError()) + ")";
        VirtualFreeEx(hProcess, remoteMemory, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
    if (!hKernel32) {
        m_error = "GetModuleHandle(kernel32) failed";
        VirtualFreeEx(hProcess, remoteMemory, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    auto pLoadLibraryW = reinterpret_cast<LPTHREAD_START_ROUTINE>(
        GetProcAddress(hKernel32, "LoadLibraryW"));
    if (!pLoadLibraryW) {
        m_error = "GetProcAddress(LoadLibraryW) failed";
        VirtualFreeEx(hProcess, remoteMemory, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    // Set environment variable so hook DLL knows the launcher PID for pipe name
    std::wstring envVar = L"MAKINEAI_HOST_PID=" + std::to_wstring(GetCurrentProcessId());
    // We pass launcher PID via environment — inject a SetEnvironmentVariableW call first
    // For simplicity, encode the PID in the pipe name using target PID instead
    // The hook DLL will connect to \\.\pipe\MakineAI_TextHook_{its own PID}

    HANDLE hThread = CreateRemoteThread(hProcess, nullptr, 0,
                                         pLoadLibraryW, remoteMemory, 0, nullptr);
    if (!hThread) {
        m_error = "CreateRemoteThread failed (error " + std::to_string(GetLastError()) + ")";
        VirtualFreeEx(hProcess, remoteMemory, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    DWORD waitResult = WaitForSingleObject(hThread, 10000);
    if (waitResult != WAIT_OBJECT_0) {
        m_error = "Remote thread timed out";
        CloseHandle(hThread);
        VirtualFreeEx(hProcess, remoteMemory, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    DWORD exitCode = 0;
    GetExitCodeThread(hThread, &exitCode);
    CloseHandle(hThread);
    VirtualFreeEx(hProcess, remoteMemory, 0, MEM_RELEASE);
    CloseHandle(hProcess);

    if (exitCode == 0) {
        m_error = "LoadLibraryW returned NULL in target process";
        return false;
    }

    return true;
}

bool HookManager::ejectDll(DWORD pid, const std::wstring& dllPath) {
    // Find the module handle in the target process
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (hSnap == INVALID_HANDLE_VALUE) return false;

    MODULEENTRY32W me{};
    me.dwSize = sizeof(me);

    HMODULE hRemoteDll = nullptr;
    if (Module32FirstW(hSnap, &me)) {
        do {
            // Compare module name to our hook DLL
            std::wstring modName = me.szModule;
            if (modName == L"makineai-hook.dll") {
                hRemoteDll = me.hModule;
                break;
            }
        } while (Module32NextW(hSnap, &me));
    }
    CloseHandle(hSnap);

    if (!hRemoteDll) return false;

    HANDLE hProcess = OpenProcess(
        PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION | PROCESS_QUERY_INFORMATION,
        FALSE, pid);
    if (!hProcess) return false;

    auto pFreeLibrary = reinterpret_cast<LPTHREAD_START_ROUTINE>(
        GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "FreeLibrary"));

    HANDLE hThread = CreateRemoteThread(hProcess, nullptr, 0,
                                         pFreeLibrary,
                                         reinterpret_cast<void*>(hRemoteDll),
                                         0, nullptr);
    if (hThread) {
        WaitForSingleObject(hThread, 5000);
        CloseHandle(hThread);
    }

    CloseHandle(hProcess);
    return true;
}

void HookManager::startPipeServer() {
    m_running = true;
    m_pipeThread = CreateThread(nullptr, 0,
        [](LPVOID param) -> DWORD {
            auto* self = static_cast<HookManager*>(param);
            self->pipeThreadFunc();
            return 0;
        }, this, 0, nullptr);
}

void HookManager::stopPipeServer() {
    m_running = false;
    if (m_pipeThread) {
        // Break the blocking ConnectNamedPipe by connecting to the pipe ourselves
        std::wstring pipeName = L"\\\\.\\pipe\\MakineAI_TextHook_"
                                + std::to_wstring(m_targetPid);
        HANDLE hDummy = CreateFileW(pipeName.c_str(), GENERIC_READ | GENERIC_WRITE,
                                     0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (hDummy != INVALID_HANDLE_VALUE)
            CloseHandle(hDummy);

        WaitForSingleObject(m_pipeThread, 3000);
        CloseHandle(m_pipeThread);
        m_pipeThread = nullptr;
    }
}

void HookManager::pipeThreadFunc() {
    std::wstring pipeName = L"\\\\.\\pipe\\MakineAI_TextHook_"
                            + std::to_wstring(m_targetPid);

    while (m_running) {
        HANDLE hPipe = CreateNamedPipeW(
            pipeName.c_str(),
            PIPE_ACCESS_INBOUND,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            1,          // max instances
            0,          // out buffer
            4096,       // in buffer
            100,        // timeout ms
            nullptr);

        if (hPipe == INVALID_HANDLE_VALUE) {
            m_error = "CreateNamedPipe failed (error " + std::to_string(GetLastError()) + ")";
            break;
        }

        // Wait for hook DLL to connect
        BOOL connected = ConnectNamedPipe(hPipe, nullptr) ||
                         (GetLastError() == ERROR_PIPE_CONNECTED);

        if (!connected || !m_running) {
            CloseHandle(hPipe);
            continue;
        }

        // Read messages from hook DLL
        char buffer[4096];
        DWORD bytesRead = 0;
        while (m_running) {
            BOOL ok = ReadFile(hPipe, buffer, sizeof(buffer) - 1, &bytesRead, nullptr);
            if (!ok || bytesRead == 0) break;

            buffer[bytesRead] = '\0';
            std::string text(buffer, bytesRead);

            // Filter by minimum length
            if (static_cast<int>(text.size()) < m_minTextLen)
                continue;

            // Deduplication check
            if (m_dedup) {
                bool isDuplicate = false;
                for (int i = 0; i < DEDUP_RING_SIZE; ++i) {
                    if (m_dedupRing[i] == text) {
                        isDuplicate = true;
                        break;
                    }
                }
                if (isDuplicate) continue;

                m_dedupRing[m_dedupIndex % DEDUP_RING_SIZE] = text;
                ++m_dedupIndex;
            }

            {
                std::lock_guard<std::mutex> lock(m_textMutex);
                m_latestText = std::move(text);
            }
        }

        DisconnectNamedPipe(hPipe);
        CloseHandle(hPipe);
    }
}

std::wstring HookManager::resolveHookDllPath() const {
    // Hook DLL is located next to the plugin DLL
    wchar_t modulePath[MAX_PATH]{};
    HMODULE hSelf = nullptr;

    // Get handle to our own module using a static function address
    GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&thisModuleAnchor),
        &hSelf);

    if (!hSelf) return {};

    GetModuleFileNameW(hSelf, modulePath, MAX_PATH);
    std::wstring path(modulePath);

    auto lastSlash = path.find_last_of(L"\\/");
    if (lastSlash == std::wstring::npos) return {};

    return path.substr(0, lastSlash + 1) + L"makineai-hook.dll";
}

} // namespace texthook
