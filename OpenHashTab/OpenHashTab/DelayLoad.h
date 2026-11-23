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
#pragma once

namespace delayload {
  // Loads the delay loaded AlgorithmsDll_*.dll from this module's own directory, so they resolve no matter what the
  // search path of the process that loaded us looks like. Has to run before the first call into an algorithm, and is
  // safe to call more than once.
  //
  // Not called from DllMain: loading a module while holding the loader lock can deadlock.
  void PreloadAlgorithmDlls();
} // namespace delayload
