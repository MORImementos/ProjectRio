// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Memory.h"

#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/BreakPoints.h"
#include "Core/System.h"

namespace API::Memory
{

void AddMemcheck(u32 addr)
{
  TMemCheck memcheck;
  memcheck.start_address = addr;
  memcheck.end_address = addr;
  Core::System::GetInstance().GetPowerPC().GetMemChecks().Add(std::move(memcheck));
}

void RemoveMemcheck(u32 addr)
{
  Core::System::GetInstance().GetPowerPC().GetMemChecks().Remove(addr);
}

u8 Read_U8(u32 addr)
{
  Core::CPUThreadGuard guard(Core::System::GetInstance());
  return PowerPC::MMU::HostRead_U8(guard, addr);
}

u16 Read_U16(u32 addr)
{
  Core::CPUThreadGuard guard(Core::System::GetInstance());
  return PowerPC::MMU::HostRead_U16(guard, addr);
}

u32 Read_U32(u32 addr)
{
  Core::CPUThreadGuard guard(Core::System::GetInstance());
  return PowerPC::MMU::HostRead_U32(guard, addr);
}

u64 Read_U64(u32 addr)
{
  Core::CPUThreadGuard guard(Core::System::GetInstance());
  return PowerPC::MMU::HostRead_U64(guard, addr);
}

s8 Read_S8(u32 addr)
{
  Core::CPUThreadGuard guard(Core::System::GetInstance());
  return PowerPC::MMU::HostRead_S8(guard, addr);
}

s16 Read_S16(u32 addr)
{
  Core::CPUThreadGuard guard(Core::System::GetInstance());
  return PowerPC::MMU::HostRead_S16(guard, addr);
}

s32 Read_S32(u32 addr)
{
  Core::CPUThreadGuard guard(Core::System::GetInstance());
  return PowerPC::MMU::HostRead_S32(guard, addr);
}

s64 Read_S64(u32 addr)
{
  Core::CPUThreadGuard guard(Core::System::GetInstance());
  return PowerPC::MMU::HostRead_S64(guard, addr);
}

float Read_F32(u32 addr)
{
  Core::CPUThreadGuard guard(Core::System::GetInstance());
  return PowerPC::MMU::HostRead_F32(guard, addr);
}

double Read_F64(u32 addr)
{
  Core::CPUThreadGuard guard(Core::System::GetInstance());
  return PowerPC::MMU::HostRead_F64(guard, addr);
}

void Write_U8(u32 addr, u8 val)
{
  Core::CPUThreadGuard guard(Core::System::GetInstance());
  PowerPC::MMU::HostWrite_U8(guard, val, addr);
}

void Write_U16(u32 addr, u16 val)
{
  Core::CPUThreadGuard guard(Core::System::GetInstance());
  PowerPC::MMU::HostWrite_U16(guard, val, addr);
}

void Write_U32(u32 addr, u32 val)
{
  Core::CPUThreadGuard guard(Core::System::GetInstance());
  PowerPC::MMU::HostWrite_U32(guard, val, addr);
}

void Write_U64(u32 addr, u64 val)
{
  Core::CPUThreadGuard guard(Core::System::GetInstance());
  PowerPC::MMU::HostWrite_U64(guard, val, addr);
}

void Write_S8(u32 addr, s8 val)
{
  Core::CPUThreadGuard guard(Core::System::GetInstance());
  PowerPC::MMU::HostWrite_S8(guard, val, addr);
}

void Write_S16(u32 addr, s16 val)
{
  Core::CPUThreadGuard guard(Core::System::GetInstance());
  PowerPC::MMU::HostWrite_S16(guard, val, addr);
}

void Write_S32(u32 addr, s32 val)
{
  Core::CPUThreadGuard guard(Core::System::GetInstance());
  PowerPC::MMU::HostWrite_S32(guard, val, addr);
}

void Write_S64(u32 addr, s64 val)
{
  Core::CPUThreadGuard guard(Core::System::GetInstance());
  PowerPC::MMU::HostWrite_S64(guard, val, addr);
}

void Write_F32(u32 addr, float val)
{
  Core::CPUThreadGuard guard(Core::System::GetInstance());
  PowerPC::MMU::HostWrite_F32(guard, val, addr);
}

void Write_F64(u32 addr, double val)
{
  Core::CPUThreadGuard guard(Core::System::GetInstance());
  PowerPC::MMU::HostWrite_F64(guard, val, addr);
}

}  // namespace API::Memory
