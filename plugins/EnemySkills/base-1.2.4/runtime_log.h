#pragma once

#include <windows.h>
#include <cstdint>

namespace TerrorEncounters {
class RuntimeLog {
    HANDLE file_ = INVALID_HANDLE_VALUE;

public:
    explicit RuntimeLog(HMODULE module) noexcept {
        wchar_t path[1024] = {};
        const DWORD length = GetModuleFileNameW(module, path, 1024);
        if (length == 0 || length >= 1024) return;
        DWORD end = length;
        while (end && path[end - 1] != L'\\' && path[end - 1] != L'/') --end;
        constexpr wchar_t name[] = L"EnemySkills.runtime.log";
        if (end + sizeof(name) / sizeof(wchar_t) > 1024) return;
        for (DWORD j = 0; j < sizeof(name) / sizeof(wchar_t); ++j) path[end + j] = name[j];
        file_ = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                            nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    }

    ~RuntimeLog() noexcept {
        if (file_ != INVALID_HANDLE_VALUE) {
            FlushFileBuffers(file_);
            CloseHandle(file_);
        }
    }

    void Write(const char* message) noexcept {
        if (file_ == INVALID_HANDLE_VALUE || message == nullptr) return;
        DWORD length = 0, written = 0;
        while (message[length]) ++length;
        WriteFile(file_, message, length, &written, nullptr);
        WriteFile(file_, "\r\n", 2, &written, nullptr);
    }

    void Hex(const char* label, uint64_t value) noexcept {
        constexpr char digits[] = "0123456789ABCDEF";
        char text[160] = {};
        uint32_t n = 0;
        while (label[n] && n < 120) { text[n] = label[n]; ++n; }
        text[n++] = '='; text[n++] = '0'; text[n++] = 'x';
        for (int shift = 60; shift >= 0; shift -= 4) text[n++] = digits[(value >> shift) & 15];
        text[n] = 0;
        Write(text);
    }

    void Wide(const char* label, const wchar_t* value) noexcept {
        Write(label);
        if (!value) { Write("<null>"); return; }
        char text[2048] = {};
        const int count = WideCharToMultiByte(CP_UTF8, 0, value, -1, text, sizeof(text), nullptr, nullptr);
        Write(count > 0 ? text : "<conversion failed>");
    }
};
}
