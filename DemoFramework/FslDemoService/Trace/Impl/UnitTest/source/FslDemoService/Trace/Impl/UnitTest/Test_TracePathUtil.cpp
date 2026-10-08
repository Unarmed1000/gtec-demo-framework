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

#include <FslBase/IO/Path.hpp>
#include <FslBase/Log/IO/LogPath.hpp>
#include <FslBase/UnitTest/Helper/TestFixtureFslBase.hpp>
#include <FslDemoService/Trace/Impl/TracePathUtil.hpp>
#include <exception>

using namespace Fsl;

namespace
{
  using Test_TracePathUtil = TestFixtureFslBase;

  // A file no run of the test creates
  const IO::Path g_missingFile("TracePathUtil-a-file-that-is-not-there.perfetto-trace");
}


TEST(Test_TracePathUtil, ToFullPath_NameOnly_FileDoesNotExist)
{
  const IO::Path currentDirectory = IO::Path::GetFullPath(IO::Path("."));

  const IO::Path result = TracePathUtil::ToFullPath(g_missingFile);

  EXPECT_EQ(IO::Path::Combine(currentDirectory, g_missingFile), result);
  EXPECT_TRUE(IO::Path::IsPathRooted(result));
}


TEST(Test_TracePathUtil, ToFullPath_RelativeDirectory_FileDoesNotExist)
{
  const IO::Path currentDirectory = IO::Path::GetFullPath(IO::Path("."));

  const IO::Path result = TracePathUtil::ToFullPath(IO::Path::Combine(IO::Path("."), g_missingFile));

  EXPECT_EQ(IO::Path::Combine(currentDirectory, g_missingFile), result);
}


TEST(Test_TracePathUtil, ToFullPath_FullPath_FileDoesNotExist)
{
  const IO::Path fullPath = IO::Path::Combine(IO::Path::GetFullPath(IO::Path(".")), g_missingFile);

  const IO::Path result = TracePathUtil::ToFullPath(fullPath);

  EXPECT_EQ(fullPath, result);
}


TEST(Test_TracePathUtil, ToFullPath_DirectoryDoesNotExist)
{
  const IO::Path path = IO::Path::Combine(IO::Path("TracePathUtil-a-directory-that-is-not-there"), g_missingFile);

  // Windows gives the full path of what is not there, the other platforms say that they can not
  try
  {
    const IO::Path result = TracePathUtil::ToFullPath(path);
    EXPECT_TRUE(IO::Path::IsPathRooted(result));
    EXPECT_EQ(g_missingFile, IO::Path::GetFileName(result));
  }
  catch (const std::exception&)
  {
    SUCCEED();
  }
}
