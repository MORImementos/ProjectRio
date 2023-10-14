// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Registers.h"

#include "Core/System.h"

#include "Core/PowerPC/ConditionRegister.h"

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

// Other registers

u32 Read_LR()
{
  return Core::System::GetInstance().GetPPCState().spr[8];
}

u32 Read_CTR()
{
  return Core::System::GetInstance().GetPPCState().spr[9];
}

u32 Read_NPC()
{
  return Core::System::GetInstance().GetPPCState().npc;
}

u32 Read_PC()
{
  return Core::System::GetInstance().GetPPCState().pc;
}

u32 Read_CR()
{
  PowerPC::ConditionRegister cr;
  return cr.Get();
}


// Not sure if this is functioning
void Write_CR(u32 newCR)
{
  PowerPC::ConditionRegister cr;
  cr.Set(newCR);
}

}  // namespace API::Registers
