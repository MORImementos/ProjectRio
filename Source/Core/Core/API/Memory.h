// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"
#include "Core/Core.h"
#include "Core/HW/Memmap.h"
#include "Core/System.h"

namespace API::Memory
{

// memchecks

void AddMemcheck(u32 addr);
void RemoveMemcheck(u32 addr);

u8 Read_U8(u32 addr);
u16 Read_U16(u32 addr);
u32 Read_U32(u32 addr);
u64 Read_U64(u32 addr);
s8 Read_S8(u32 addr);
s16 Read_S16(u32 addr);
s32 Read_S32(u32 addr);
s64 Read_S64(u32 addr);
float Read_F32(u32 addr);
double Read_F64(u32 addr);
// memory writing: arguments of write functions are swapped (address first) to be consistent with other scripting APIs
void Write_U8(u32 addr, u8 val);
void Write_U16(u32 addr, u16 val);
void Write_U32(u32 addr, u32 val);
void Write_U64(u32 addr, u64 val);
void Write_S8(u32 addr, s8 val);
void Write_S16(u32 addr, s16 val);
void Write_S32(u32 addr, s32 val);
void Write_S64(u32 addr, s64 val);
void Write_F32(u32 addr, float val);
void Write_F64(u32 addr, double val);

}  // namespace API::Memory
