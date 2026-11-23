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

#include <delayimp.h>

#include <cstring>
#include <string>
#include <string_view>

namespace delayload {

  namespace {

    FARPROC WINAPI DelayLoadNotify(unsigned notify, PDelayLoadInfo info);

    // Full path of this DLL, so the algorithm DLLs can be loaded from next to it. Resolved through our own code address
    // so it works no matter how or from where the shell loaded us.
    std::wstring GetOwnPath() {
      HMODULE module = nullptr;
      if (!GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&DelayLoadNotify),
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
          return buffer;
        }
        buffer.resize(buffer.size() * 2);
      }
    }

    std::wstring GetOwnDirectory() {
      auto path = GetOwnPath();
      if (path.empty())
        return {};

      const auto slash = path.rfind(L'\\');
      if (slash == std::wstring::npos)
        return {};

      path.resize(slash + 1);
      return path;
    }

    std::wstring Utf8ToWide(const char* str) {
      const auto len = static_cast<int>(strlen(str));
      if (len <= 0)
        return {};

      const auto size = MultiByteToWideChar(CP_UTF8, 0, str, len, nullptr, 0);
      if (size <= 0)
        return {};

      std::wstring out(static_cast<size_t>(size), L'\0');
      if (MultiByteToWideChar(CP_UTF8, 0, str, len, out.data(), size) != size)
        return {};

      return out;
    }

    FARPROC WINAPI DelayLoadNotify(unsigned notify, PDelayLoadInfo info) {
      if (notify != dliNotePreLoadLibrary || !info || !info->szDll)
        return nullptr;

      // Only our own algorithm DLLs are shipped next to us, everything else (winhttp) is a system module
      constexpr std::string_view k_algorithm_prefix = "AlgorithmsDll_";
      if (std::string_view{info->szDll}.rfind(k_algorithm_prefix, 0) != 0)
        return nullptr;

      // Both are constant for the lifetime of the module
      static const auto directory = GetOwnDirectory();
      if (directory.empty())
        return nullptr;

      const auto dll_name = Utf8ToWide(info->szDll);
      if (dll_name.empty())
        return nullptr;

      return reinterpret_cast<FARPROC>(LoadLibraryW((directory + dll_name).c_str()));
    }

  } // namespace

  void InstallAlgorithmHook() {
    // delayimp.lib owns this pointer, and every module that links it gets its own copy, so this only affects our own
    // delay loaded imports.
    __pfnDliNotifyHook2 = &DelayLoadNotify;
  }

} // namespace delayload
