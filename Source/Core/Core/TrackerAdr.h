#pragma once

#include <type_traits>
#include <optional>
#include <ostream>

//For Mem Access
// #include "Core/HW/Memmap.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PowerPC.h"

template <typename T>
class TrackerValue {
public:
    TrackerValue() {};
    TrackerValue(std::string in_name, T in_default_value)
    {
        name = in_name;
        default_value = in_default_value;
    };
    
    std::string name;
    std::optional<T> value;
    std::optional<T> prev_value;

    T default_value;

    T get_value(){
        if (value.has_value()) { return value.value(); }
        return default_value;
    }

    void set_value(T new_value){
        prev_value=value;
        value=new_value;
    }

    void set_value_to_prev(){
        value=prev_value;
    }

    std::pair<std::string, std::string> get_key_value_string(){
        return std::make_pair(name, std::to_string(get_value()));
    }

    void write(std::ostream stream, std::string sep=" "){
        stream << name << sep << std::to_string(get_value());
    }
};

template <typename T>
class TrackerAdr : public TrackerValue<T>{
public:
    TrackerAdr() {};
    TrackerAdr(std::string in_name, u32 in_adr, T in_default_value) :
        TrackerValue<T>(in_name, in_default_value),
        adr(in_adr)
    {
        static_assert((std::is_same<T, u8>::value || std::is_same<T, u16>::value || std::is_same<T, u32>::value), "TrackerAdr type is not valid. Must be u8, u16, or u32");
    }

    u32 adr;

    T read_value(const Core::CPUThreadGuard& guard) {
        T mem_val;
        if constexpr(std::is_same<T, u8>::value){
            mem_val = PowerPC::MMU::HostRead_U8(guard, adr);
        }
        else if constexpr(std::is_same<T, u16>::value){
            mem_val = PowerPC::MMU::HostRead_U16(guard, adr);
        }
        else if constexpr(std::is_same<T, u32>::value){
            mem_val = PowerPC::MMU::HostRead_U32(guard, adr);
        }
        TrackerValue<T>::set_value(mem_val);
        return mem_val;
    }
};

//ostream& operator<<(ostream& os, const TrackerValue<T>& dt)
