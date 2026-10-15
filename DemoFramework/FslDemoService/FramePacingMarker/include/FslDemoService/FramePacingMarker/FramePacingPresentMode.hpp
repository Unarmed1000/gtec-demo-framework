#ifndef FSLDEMOSERVICE_FRAMEPACINGMARKER_FRAMEPACINGPRESENTMODE_HPP
#define FSLDEMOSERVICE_FRAMEPACINGMARKER_FRAMEPACINGPRESENTMODE_HPP
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

namespace Fsl
{
  //! How the side of the display takes presents, in no graphics API's words (FramePacingSwapchainEnvironment).
  enum class FramePacingPresentMode
  {
    //! Not known
    Unknown,
    //! Every present is shown, in order, one a refresh at the most; a full queue makes the present wait (Vulkan's FIFO, a GL swap
    //! interval of one or more)
    Queue,
    //! The same, but a present that missed its refresh is shown at once and may tear (Vulkan's FIFO relaxed, a GL swap interval of -1)
    QueueLateAtOnce,
    //! At each refresh the newest finished present is shown and older ones are left out; nothing tears (Vulkan's mailbox and FIFO latest
    //! ready)
    NewestAtRefresh,
    //! A present is shown at once, and tears (Vulkan's immediate, a swap interval of 0)
    Immediate,
  };
}

#endif
