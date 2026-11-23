//    Copyright 2019-2025 namazso <admin@namazso.eu>
//    This file is part of OpenHashTab.
//
//    OpenHashTab is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    OpenHashTab is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with OpenHashTab.  If not, see <https://www.gnu.org/licenses/>.
#include "DelayLoad.h"

#include <string>

namespace delayload {

  namespace {

    // The algorithm DLLs are delay loaded by bare name, so the delay load helper resolves them through the search path of
    // whatever process loaded us. In Explorer that is C:\Windows and friends, never our installation directory, and a
    // failed delay load surfaces as 0xC06D007E, which Explorer cannot survive.
    //
    // Loading them ourselves by full path fixes that without touching the process: a later bare name lookup finds the
    // already loaded module by its base name and does not search at all.

    // Full path of this DLL, resolved through our own code address so it works no matter how we were loaded.
    std::wstring GetOwnPath() {
      HMODULE module = nullptr;
      if (!GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&GetOwnPath),
            &module
          ))
        return {};

      std::wstring buffer(MAX_PATH, L'\0');
      while (true) {
        const auto len = GetModuleFileNameW(module, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (len == 0)
          return {};
        if (len < buffer.size()) {
          buffer.resize(len);
          break;
        }
        buffer.resize(buffer.size() * 2);
      }

      return buffer;
    }

    // Directory part of a path, trailing slash included, or empty if there is no directory part
    std::wstring GetDirectoryOf(const std::wstring& path) {
      const auto slash = path.rfind(L'\\');
      if (slash == std::wstring::npos)
        return {};

      return path.substr(0, slash + 1);
    }

    bool Preload(const std::wstring& directory) {
      // Every flavor this build produced, no need to know which one the code is going to pick
      const auto pattern = directory + L"AlgorithmsDll_*.dll";
      WIN32_FIND_DATAW find{};
      const auto handle = FindFirstFileW(pattern.c_str(), &find);
      if (handle == INVALID_HANDLE_VALUE)
        return false;

      do {
        if (find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
          continue;
        // The reference is deliberately kept: the module has to stay loaded for the delay load helper to find it
        LoadLibraryW((directory + find.cFileName).c_str());
      } while (FindNextFileW(handle, &find) != 0);

      FindClose(handle);
      return true;
    }

    bool PreloadOnce() {
      const auto directory = GetDirectoryOf(GetOwnPath());
      if (directory.empty())
        return false;

      return Preload(directory);
    }

  } // namespace

  void PreloadAlgorithmDlls() {
    // A bool has no destructor, so this does not give the DLL an exit time destructor
    static const bool once = PreloadOnce();
    (void)once;
  }

} // namespace delayload
