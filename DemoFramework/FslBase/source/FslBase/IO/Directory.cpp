/****************************************************************************************************************************************************
 * Copyright (c) 2014 Freescale Semiconductor, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *    * Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *
 *    * Redistributions in binary form must reproduce the above copyright notice,
 *      this list of conditions and the following disclaimer in the documentation
 *      and/or other materials provided with the distribution.
 *
 *    * Neither the name of the Freescale Semiconductor, Inc. nor the names of
 *      its contributors may be used to endorse or promote products derived from
 *      this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************************************************************************************/

#include <FslBase/Exceptions.hpp>
#include <FslBase/IO/Directory.hpp>
#include <FslBase/IO/File.hpp>
#include <FslBase/System/Platform/PlatformFileSystem.hpp>
#include <cassert>
#include "../System/Platform/Platform.hpp"

namespace Fsl::IO
{
  namespace
  {
    void RecursiveCreate(const Path& path)
    {
      auto tmpPath = Path::GetDirectoryName(path);
      if (tmpPath.GetByteSize() > 0)
      {
        RecursiveCreate(tmpPath);
      }

      if (!path.EndsWith(":"))
      {
        PlatformFileSystem::CreateDir(path);
      }
    }
  }

  void Directory::CreateDir(const Path& path)
  {
    if (path.EndsWith(":"))
    {
      throw IOException("Invalid path name");
    }

    const auto indexOfSlash = path.IndexOf('/');
    if (indexOfSlash >= 0 && path.IndexOf(':', indexOfSlash) > 0)
    {
      throw NotSupportedException("A path can only contain ':' in the drive label");
    }

    RecursiveCreate(path);
  }


  bool Directory::Exists(const Path& path)
  {
    FileAttributes attr;
    if (!File::TryGetAttributes(path, attr))
    {
      return false;
    }
    return (attr.HasFlag(FileAttributes::Directory));
  }


  Path Directory::GetCurrentWorkingDirectory()
  {
    return Path(Platform::GetCurrentWorkingDirectory());
  }


  void Directory::GetFiles(PathDeque& rResult, const Path& path, const SearchOptions searchOptions)
  {
    return PlatformFileSystem::GetFiles(rResult, path, searchOptions);
  }


  bool Directory::TryGetFiles(PathDeque& rResult, const Path& path, const SearchOptions searchOptions)
  {
    try
    {
      GetFiles(rResult, path, searchOptions);
      return true;
    }
    catch (const std::exception&)
    {
      return false;
    }
  }
}
