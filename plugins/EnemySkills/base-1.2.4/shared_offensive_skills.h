#pragma once


namespace SharedOffensive {
inline bool Supported(uint32_t id) noexcept {
 switch(id){case 44:case 48:case 92:case 388:case 394:case 398:case 401:case 402:return true;default:return false;}
}
inline bool Nova(uint32_t id) noexcept {return id==44||id==48||id==92||id==470;}
}
