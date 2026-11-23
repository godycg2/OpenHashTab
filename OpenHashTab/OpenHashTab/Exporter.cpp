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
#include "Exporter.h"

#include "ExportFormat.h"
#include "FileHashTask.h"
#include "Settings.h"
#include "utl.h"

#include <algorithm>

// Collects everything the formatter needs from the tasks, in the order they get written out.
// The texts themselves are built by exportformat, so both exporters and the clipboard use one implementation.
static std::vector<exportformat::SumfileEntry> CollectEntries(const std::list<FileHashTask*>& files) {
  std::vector<FileHashTask*> files_copy{files.cbegin(), files.cend()};
  std::sort(files_copy.begin(), files_copy.end(), [](const FileHashTask* a, const FileHashTask* b) {
    return a->GetDisplayName() < b->GetDisplayName();
  });

  std::vector<exportformat::SumfileEntry> entries;
  entries.reserve(files_copy.size());
  for (const auto file : files_copy) {
    exportformat::SumfileEntry entry;
    entry.filename = utl::WideToUTF8(file->GetDisplayName().c_str());
    entry.error = file->GetError() != ERROR_SUCCESS;
    entry.source_time = file->GetSourceTime();
    const auto& results = file->GetHashResult();
    entry.hashes.assign(results.cbegin(), results.cend());
    entries.push_back(std::move(entry));
  }
  return entries;
}

static size_t AlgorithmIndexByName(const char* name) {
  const auto idx = LegacyHashAlgorithm::IdxByName(name);
  // IdxByName returns -1 when the algorithm does not exist
  return idx < 0 ? 0 : static_cast<size_t>(idx);
}

class SumfileExporter : public Exporter {
  size_t idx;

public:
  constexpr SumfileExporter(size_t idx)
      : idx(idx) {}

  bool IsEnabled(Settings* settings) const override { return settings->algorithms[idx]; }

  std::string GetExportString(Settings* settings, bool for_clipboard, const std::list<FileHashTask*>& files) const override;

  [[nodiscard]] const char* GetName() const override { return LegacyHashAlgorithm::Algorithms()[idx].GetName(); }

  [[nodiscard]] const char* GetExtension() const override {
    const auto v = LegacyHashAlgorithm::Algorithms()[idx].GetExtensions()[0];
    return v ? v : "sums";
  }
};

class SFVExporter : public Exporter {
public:
  constexpr SFVExporter() = default;

  [[nodiscard]] const char* GetName() const override { return "SFV (CRC32)"; }

  bool IsEnabled(Settings* settings) const override { return settings->algorithms[LegacyHashAlgorithm::IdxByName("CRC32")]; }

  std::string GetExportString(Settings* settings, bool for_clipboard, const std::list<FileHashTask*>& files) const override;

  [[nodiscard]] const char* GetExtension() const override { return "sfv"; }
};

class DotHashExporter : public Exporter {
public:
  constexpr DotHashExporter() = default;

  [[nodiscard]] const char* GetName() const override { return ".hash (corz)"; }

  bool IsEnabled(Settings* settings) const override { return true; }

  std::string GetExportString(Settings* settings, bool for_clipboard, const std::list<FileHashTask*>& files) const override;

  [[nodiscard]] const char* GetExtension() const override { return "hash"; }
};

std::string SFVExporter::GetExportString(
  Settings* settings,
  bool for_clipboard,
  const std::list<FileHashTask*>& files
) const {
  exportformat::SumfileOptions options;
  options.for_clipboard = for_clipboard;
  options.algorithm = AlgorithmIndexByName("CRC32");
  options.unix_endings = settings->sumfile_unix_endings;
  options.uppercase = settings->sumfile_uppercase;
  options.forward_slashes = settings->sumfile_forward_slashes;
  options.banner = settings->sumfile_banner;
  options.banner_date = settings->sumfile_banner_date;

  // The operation time is taken once, so every timestamp in one export agrees with the others
  return exportformat::BuildSfvText(
    CollectEntries(files),
    options,
    CI_VERSION,
    exportformat::Now()
  );
}

static std::string GetExportStringSumfile(
  Settings* settings,
  bool for_clipboard,
  const std::list<FileHashTask*>& files,
  size_t algorithm,
  bool dot_hash
) {
  exportformat::SumfileOptions options;
  options.for_clipboard = for_clipboard;
  options.dot_hash = dot_hash;
  options.algorithm = algorithm;
  options.unix_endings = settings->sumfile_unix_endings;
  options.uppercase = !dot_hash && settings->sumfile_uppercase;
  options.double_space = settings->sumfile_use_double_space;
  options.forward_slashes = !dot_hash && settings->sumfile_forward_slashes;
  options.dot_hash_compatible = dot_hash || settings->sumfile_dot_hash_compatible;
  options.banner = settings->sumfile_banner;
  options.banner_date = settings->sumfile_banner_date;

  for (auto i = 0u; i < LegacyHashAlgorithm::k_count; ++i) {
    options.hash_names.emplace_back(LegacyHashAlgorithm::Algorithms()[i].GetName());
    options.enabled_algorithms.push_back(settings->algorithms[i]);
  }

  // The operation time is taken once, so every timestamp in one export agrees with the others
  return exportformat::BuildSumfileText(
    CollectEntries(files),
    options,
    CI_VERSION,
    exportformat::Now()
  );
}

std::string SumfileExporter::GetExportString(
  Settings* settings,
  bool for_clipboard,
  const std::list<FileHashTask*>& files
) const {
  return GetExportStringSumfile(settings, for_clipboard, files, idx, false);
}

std::string DotHashExporter::GetExportString(
  Settings* settings,
  bool for_clipboard,
  const std::list<FileHashTask*>& files
) const {
  // In dot_hash mode every enabled algorithm is written, the index is not used
  return GetExportStringSumfile(settings, for_clipboard, files, 0, true);
}

template <typename T, size_t N, size_t... Rest>
struct Array_impl {
  static constexpr auto& value = Array_impl<T, N - 1, N, Rest...>::value;
};

template <typename T, size_t... Rest>
struct Array_impl<T, 0, Rest...> {
  static constexpr SumfileExporter value[] = {0, Rest...};
};

template <typename T, size_t N>
struct Array {
  static constexpr auto& value = Array_impl<T, N>::value;

  Array() = delete;
  Array(const Array&) = delete;
  Array(Array&&) = delete;
};

static constexpr auto s_dot_hash_exporter = DotHashExporter();
static constexpr auto s_sfv_exporter = SFVExporter();

constexpr std::array<const Exporter*, Exporter::k_count> Exporter::k_exporters = [] {
  static_assert(Exporter::k_count == LegacyHashAlgorithm::k_count + 2, "Wrong exporter count");
  std::array<const Exporter*, k_count> elems{};
  auto i = 0u;
  for (; i < LegacyHashAlgorithm::k_count; ++i)
    elems[i] = &Array<SumfileExporter, LegacyHashAlgorithm::k_count>::value[i];
  elems[LegacyHashAlgorithm::k_count] = &s_dot_hash_exporter;
  elems[LegacyHashAlgorithm::k_count + 1] = &s_sfv_exporter;
  return elems;
}();
