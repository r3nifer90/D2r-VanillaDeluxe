#pragma once


namespace AreaPolicy {
enum class Kind { Normal, Terror, Map };
constexpr bool IsMap(uint32_t id) noexcept {
 return id==146 || (id>=149 && id<=156) || (id>=159 && id<=164);
}
constexpr Kind Classify(uint32_t id,bool terror) noexcept {
 return IsMap(id)?Kind::Map:terror?Kind::Terror:Kind::Normal;
}
constexpr const char* Label(Kind kind) noexcept {
 return kind==Kind::Map?"maps":kind==Kind::Terror?"terror":"normal";
}
}
