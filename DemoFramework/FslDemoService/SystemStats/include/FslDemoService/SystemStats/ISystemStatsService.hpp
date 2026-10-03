#ifndef FSLDEMOSERVICE_SYSTEMSTATS_ISYSTEMSTATSSERVICE_HPP
#define FSLDEMOSERVICE_SYSTEMSTATS_ISYSTEMSTATSSERVICE_HPP
//****************************************************************************************************************************************************
//* BSD 3-Clause License
//*
//* Copyright (c) 2026, Mana Battery
//* All rights reserved.
//*
//* Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
//*
//* 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
//* 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the
//*    documentation and/or other materials provided with the distribution.
//* 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this
//*    software without specific prior written permission.
//*
//* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
//* THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
//* CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
//* PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
//* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
//* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//****************************************************************************************************************************************************


#include <FslDemoService/CpuStats/ICpuStatsService.hpp>
#include <FslDemoService/SystemStats/GpuMemoryUsageRecord.hpp>
#include <FslDemoService/SystemStats/GpuUsageRecord.hpp>

namespace Fsl
{
  //! What the system knows about the resources the application uses: the CPU and the RAM (the methods of ICpuStatsService) and the GPU.
  //!
  //! The GPU numbers come from the operating system, so they are the same for every graphics API. They are optional: a platform or a driver
  //! that does not have them makes the method return false. Measuring starts with the first call and the first result takes a few seconds,
  //! so a application that never asks pays nothing.
  class ISystemStatsService : public ICpuStatsService
  {
  public:
    //! @brief Get how busy the GPU is with the work of the application: the load of the busiest GPU engine it uses.
    //! @return false if not available (the record is then cleared).
    virtual bool TryGetApplicationGpuUsage(GpuUsageRecord& rUsageRecord) const = 0;

    //! @brief Get how much GPU memory the application uses.
    //! @return false if not available (the record is then cleared).
    virtual bool TryGetApplicationGpuMemoryUsage(GpuMemoryUsageRecord& rUsageRecord) const = 0;
  };
}

#endif
