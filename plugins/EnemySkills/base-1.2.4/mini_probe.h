#pragma once


namespace MiniProbe {
inline constexpr bool Available() noexcept {return false;}
inline void Line(const char*) noexcept {}
inline void Open(const wchar_t*,const wchar_t*) noexcept {}
inline void Close() noexcept {}
}
