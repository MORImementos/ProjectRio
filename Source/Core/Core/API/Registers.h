// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"
#include "Core/PowerPC/PowerPC.h"

namespace API::Registers
{

// Register reading

u32 Read_GPR(u32 index);

double Read_FPR(u32 index);

// Register writing

void Write_GPR(u32 index, u32 value);

void Write_FPR(u32 index, double value);

// Other registers

u32 Read_LR();

u32 Read_CTR();

u32 Read_NPC();

u32 Read_PC();

u32 Read_CR();

void Write_CR(u32 newCR);

}  // namespace API::Registers
