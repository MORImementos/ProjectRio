// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Registers.h"

#include "Core/System.h"

namespace API::Registers
{

// Register reading

u32 Read_GPR(u32 index)
{
  return Core::System::GetInstance().GetPPCState().gpr[index];
}

double Read_FPR(u32 index)
{
  return Core::System::GetInstance().GetPPCState().ps[index].PS0AsDouble();
}

// register writing

void Write_GPR(u32 index, u32 value)
{
  Core::System::GetInstance().GetPPCState().gpr[index] = value;
}

void Write_FPR(u32 index, double value)
{
  Core::System::GetInstance().GetPPCState().ps[index].SetPS0(value);
}

}  // namespace API::Registers
