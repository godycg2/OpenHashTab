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

#include <cstddef>
#include <cstdint>
#include <ctime>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Text formatting for exported checksum files (and for the same text put on the clipboard).
//
// This module intentionally depends on nothing but the C++ standard library. The timestamps embedded in the output used
// to be produced (and hardcoded) at this layer, so keeping it free of Win32 lets the generated text be unit tested.
namespace exportformat {

  using UnixSeconds = int64_t;

  // FILETIME is a count of 100ns intervals since 1601-01-01T00:00:00Z
  inline constexpr UnixSeconds k_filetime_ticks_per_second = 10'000'000;
  // Seconds between the FILETIME epoch and the Unix epoch
  inline constexpr UnixSeconds k_filetime_unix_epoch_delta = 11'644'473'600;

  // Converts a FILETIME quad (100ns since 1601) to seconds since the Unix epoch.
  // Returns nullopt when the value is unset (zero), which is what callers see when the timestamp could not be read.
  std::optional<UnixSeconds> FileTimeToUnixSeconds(uint64_t file_time);

  // Wall clock time the operation happens at, in seconds since the Unix epoch.
  UnixSeconds Now();

  // Broken down time conversion. Returns false if the value cannot be represented.
  bool ToUtcTm(UnixSeconds time, std::tm& out);
  bool ToLocalTm(UnixSeconds time, std::tm& out);

  // "2011-10-08T07:07:09Z"
  std::string FormatISO8601(const std::tm& utc_time);

  // Local timestamp in the format corz checksum uses in .hash files: "2011.10.08@07.07:09"
  std::string FormatDotHashTime(const std::tm& local_time);

  // ISO 8601 UTC timestamp of the operation, for the file banner.
  std::string FormatBannerTimestamp(UnixSeconds operation_time);

  // Timestamp of a .hash compatible metadata line: the time of the source when it has one, otherwise the time the
  // operation happened at.
  std::string FormatDotHashTimestamp(
    const std::optional<UnixSeconds>& source_time,
    UnixSeconds operation_time
  );

  struct SumfileEntry {
    // Path as displayed to the user, UTF-8 encoded
    std::string filename;

    // Hexed by the formatter, indexed by algorithm index. Empty vector means the algorithm produced no result.
    std::vector<std::vector<uint8_t>> hashes;

    // Failed files are skipped, just like before
    bool error = false;

    // Last write time of the hashed source, if it could be read
    std::optional<UnixSeconds> source_time;
  };

  struct SumfileOptions {
    // Clipboard copies must never contain a banner
    bool for_clipboard = false;

    // The ".hash (corz)" exporter, which writes every enabled algorithm and always uses CRLF
    bool dot_hash = false;

    // Algorithm index the sumfile is for
    size_t algorithm = 0;

    bool unix_endings = true;
    bool uppercase = true;
    bool double_space = false;
    bool forward_slashes = true;
    bool dot_hash_compatible = true;
    bool banner = true;
    bool banner_date = false;

    // Algorithm names by index, used for the .hash metadata lines
    std::vector<std::string> hash_names;

    // Enabled algorithms by index, used when dot_hash is set
    std::vector<bool> enabled_algorithms;
  };

  // Builds a sumfile (GNU coreutils / BSD style), optionally with corz .hash compatible metadata lines.
  // Files are written in the given order.
  std::string BuildSumfileText(
    const std::vector<SumfileEntry>& files,
    const SumfileOptions& options,
    std::string_view version,
    UnixSeconds operation_time
  );

  // Builds an SFV file, using options.algorithm as the checksum index.
  std::string BuildSfvText(
    const std::vector<SumfileEntry>& files,
    const SumfileOptions& options,
    std::string_view version,
    UnixSeconds operation_time
  );

} // namespace exportformat
