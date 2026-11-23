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

// Regression tests for exported / copied timestamps.
//
// The bug these cover: the .hash compatible metadata lines written on export
// and on "copy to clipboard" carried a hardcoded 1970.01.01@00.00:00 instead of
// the timestamp of the hashed file.
#include "ExportFormat.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

namespace {

int g_failures = 0;
int g_checks = 0;

// Renders control characters so a mismatch in line endings is visible in the
// output
std::string Escape(const std::string &str) {
  std::string out;
  for (const auto c : str) {
    switch (c) {
    case '\r':
      out += "\\r";
      break;
    case '\n':
      out += "\\n";
      break;
    case '\t':
      out += "\\t";
      break;
    default:
      out += c;
      break;
    }
  }
  return out;
}

void ReportFailure(const char *file, int line, const char *what,
                   const std::string &detail) {
  ++g_failures;
  std::printf("FAIL %s:%d: %s\n  %s\n", file, line, what, detail.c_str());
}

void Check(bool condition, const char *expr, const char *file, int line) {
  ++g_checks;
  if (!condition)
    ReportFailure(file, line, expr, "condition is false");
}

void CheckString(const std::string &actual, const std::string &expected,
                 const char *expr, const char *file, int line) {
  ++g_checks;
  if (actual != expected)
    ReportFailure(file, line, expr,
                  "expected:\n  \"" + Escape(expected) + "\"\n  actual:\n  \"" +
                      Escape(actual) + "\"");
}

void CheckNotContains(const std::string &haystack, const std::string &needle,
                      const char *expr, const char *file, int line) {
  ++g_checks;
  if (haystack.find(needle) != std::string::npos)
    ReportFailure(file, line, expr,
                  "\"" + Escape(needle) + "\" unexpectedly found in:\n  \"" +
                      Escape(haystack) + "\"");
}

#define CHECK(expr) Check((expr), #expr, __FILE__, __LINE__)
#define CHECK_STR(actual, expected)                                            \
  CheckString((actual), (expected), #actual, __FILE__, __LINE__)
#define CHECK_NOT_CONTAINS(haystack, needle)                                   \
  CheckNotContains((haystack), (needle),                                       \
                   #haystack " does not contain " #needle, __FILE__, __LINE__)

// 2011-10-08T07:07:09Z
constexpr exportformat::UnixSeconds k_source_time = 1318057629;
// 2026-11-23T16:33:00Z, the moment the "user" copied or exported
constexpr exportformat::UnixSeconds k_operation_time = 1795451580;

// The same instant as k_source_time as a FILETIME, with some sub-second noise
// that must be truncated away
constexpr uint64_t SourceFileTime() {
  return (static_cast<uint64_t>(k_source_time) +
          static_cast<uint64_t>(exportformat::k_filetime_unix_epoch_delta)) *
             static_cast<uint64_t>(exportformat::k_filetime_ticks_per_second) +
         1234567;
}

constexpr exportformat::UnixSeconds k_1960 = -315521755; // 1960-01-02T03:04:05Z

std::tm MakeTm(int year, int month, int day, int hour, int minute, int second) {
  std::tm tm{};
  tm.tm_year = year - 1900;
  tm.tm_mon = month - 1;
  tm.tm_mday = day;
  tm.tm_hour = hour;
  tm.tm_min = minute;
  tm.tm_sec = second;
  return tm;
}

// Makes local time equal UTC so the .hash timestamps are predictable across
// machines
void ForceUtcTimezone() {
#ifdef _WIN32
  _putenv_s("TZ", "UTC0");
  _tzset();
#else
  setenv("TZ", "UTC0", 1);
  tzset();
#endif
}

std::vector<uint8_t> Bytes(std::initializer_list<uint8_t> bytes) {
  return std::vector<uint8_t>{bytes};
}

exportformat::SumfileEntry
MakeEntry(std::string filename, std::vector<std::vector<uint8_t>> hashes,
          std::optional<exportformat::UnixSeconds> source_time = std::nullopt) {
  exportformat::SumfileEntry entry;
  entry.filename = std::move(filename);
  entry.hashes = std::move(hashes);
  entry.source_time = source_time;
  return entry;
}

exportformat::SumfileOptions MakeOptions() {
  exportformat::SumfileOptions options;
  options.hash_names = {"MD5", "SHA-256"};
  options.enabled_algorithms = {true, true};
  options.algorithm = 1; // SHA-256
  return options;
}

const auto k_hash_md5 = Bytes({0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF});
const auto k_hash_sha256 = Bytes({0xDE, 0xAD, 0xBE, 0xEF});

// Files used by the export/clipboard tests: one with a known source timestamp,
// one without any
std::vector<exportformat::SumfileEntry> MakeTestFiles() {
  return {
      MakeEntry("dir\\a.bin", {k_hash_md5, k_hash_sha256}, k_source_time),
      MakeEntry("b.bin", {k_hash_md5, k_hash_sha256}),
  };
}

void TestFileTimeConversion() {
  CHECK(!exportformat::FileTimeToUnixSeconds(0).has_value());

  const auto converted = exportformat::FileTimeToUnixSeconds(SourceFileTime());
  CHECK(converted.has_value());
  CHECK(converted.value() == k_source_time);

  // Before the Unix epoch the result must stay negative instead of wrapping
  // around
  const auto before_epoch = exportformat::FileTimeToUnixSeconds(
      (static_cast<uint64_t>(k_1960) +
       exportformat::k_filetime_unix_epoch_delta) *
      10000000);
  CHECK(before_epoch.has_value());
  CHECK(before_epoch.value() == k_1960);

  // A FILETIME pointing at the Unix epoch itself
  const auto epoch = exportformat::FileTimeToUnixSeconds(
      exportformat::k_filetime_unix_epoch_delta * 10000000);
  CHECK(epoch.has_value());
  CHECK(epoch.value() == 0);
}

void TestTimeFormatting() {
  CHECK_STR(exportformat::FormatISO8601(MakeTm(2011, 10, 8, 7, 7, 9)),
            "2011-10-08T07:07:09Z");
  CHECK_STR(exportformat::FormatDotHashTime(MakeTm(2011, 10, 8, 7, 7, 9)),
            "2011.10.08@07.07:09");

  // Zero padding of every field
  CHECK_STR(exportformat::FormatDotHashTime(MakeTm(2000, 1, 2, 3, 4, 5)),
            "2000.01.02@03.04:05");
  CHECK_STR(exportformat::FormatISO8601(MakeTm(2000, 1, 2, 3, 4, 5)),
            "2000-01-02T03:04:05Z");
}

void TestDotHashTimestampSelection() {
  // The source time wins when there is one
  CHECK_STR(
      exportformat::FormatDotHashTimestamp(k_source_time, k_operation_time),
      "2011.10.08@07.07:09");

  // Without a source time the operation time is used, never the epoch
  CHECK_STR(
      exportformat::FormatDotHashTimestamp(std::nullopt, k_operation_time),
      "2026.11.23@16.33:00");
  CHECK_NOT_CONTAINS(
      exportformat::FormatDotHashTimestamp(std::nullopt, k_operation_time),
      "1970.01.01");

  // Same for the banner, which is always UTC
  CHECK_STR(exportformat::FormatBannerTimestamp(k_operation_time),
            "2026-11-23T16:33:00Z");
}

// Exporting to a file: banner (with date) plus .hash compatible metadata lines
void TestExportToFileTimestamps() {
  auto options = MakeOptions();
  options.banner_date = true;
  options.for_clipboard = false;

  const auto text = exportformat::BuildSumfileText(MakeTestFiles(), options,
                                                   "1.2.3", k_operation_time);

  const std::string expected =
      "# Generated by OpenHashTab 1.2.3 at 2026-11-23T16:33:00Z\n"
      "# https://github.com/namazso/OpenHashTab/\n"
      "#\n"
      "#sha256#dir\\a.bin#2011.10.08@07.07:09\n"
      "DEADBEEF *dir/a.bin\n"
      "#sha256#b.bin#2026.11.23@16.33:00\n"
      "DEADBEEF *b.bin\n";
  CHECK_STR(text, expected);

  CHECK_NOT_CONTAINS(text, "1970.01.01");
  CHECK_NOT_CONTAINS(text, "@00.00:00");
}

// The same text, but as the clipboard copy takes it: no banner, CRLF,
// timestamps of the sources
void TestClipboardTimestamps() {
  auto options = MakeOptions();
  options.banner_date = true;
  options.for_clipboard = true;

  const auto text = exportformat::BuildSumfileText(MakeTestFiles(), options,
                                                   "1.2.3", k_operation_time);

  const std::string expected = "#sha256#dir\\a.bin#2011.10.08@07.07:09\r\n"
                               "DEADBEEF *dir/a.bin\r\n"
                               "#sha256#b.bin#2026.11.23@16.33:00\r\n"
                               "DEADBEEF *b.bin\r\n";
  CHECK_STR(text, expected);

  CHECK_NOT_CONTAINS(text, "1970.01.01");
  CHECK_NOT_CONTAINS(text, "Generated by");
}

// Without any source time at all, both operations must still report the moment
// they happened
void TestOperationTimeFallback() {
  const std::vector<exportformat::SumfileEntry> files = {
      MakeEntry("only.bin", {k_hash_md5, k_hash_sha256}),
  };

  auto clipboard_options = MakeOptions();
  clipboard_options.for_clipboard = true;
  CHECK_STR(exportformat::BuildSumfileText(files, clipboard_options, "1.2.3",
                                           k_operation_time),
            "#sha256#only.bin#2026.11.23@16.33:00\r\nDEADBEEF *only.bin\r\n");

  auto file_options = MakeOptions();
  file_options.banner_date = true;
  CHECK_STR(exportformat::BuildSumfileText(files, file_options, "1.2.3",
                                           k_operation_time),
            "# Generated by OpenHashTab 1.2.3 at 2026-11-23T16:33:00Z\n"
            "# https://github.com/namazso/OpenHashTab/\n"
            "#\n"
            "#sha256#only.bin#2026.11.23@16.33:00\n"
            "DEADBEEF *only.bin\n");
}

// The ".hash (corz)" exporter writes every enabled algorithm, always CRLF,
// keeping the raw slashes
void TestDotHashExporterTimestamps() {
  auto options = MakeOptions();
  options.dot_hash = true;
  options.for_clipboard = true; // keeps the banner out of the way
  const auto text = exportformat::BuildSumfileText(MakeTestFiles(), options,
                                                   "1.2.3", k_operation_time);

  const std::string expected = "#md5#dir\\a.bin#2011.10.08@07.07:09\r\n"
                               "0123456789ABCDEF *dir\\a.bin\r\n"
                               "#sha256#dir\\a.bin#2011.10.08@07.07:09\r\n"
                               "DEADBEEF *dir\\a.bin\r\n"
                               "#md5#b.bin#2026.11.23@16.33:00\r\n"
                               "0123456789ABCDEF *b.bin\r\n"
                               "#sha256#b.bin#2026.11.23@16.33:00\r\n"
                               "DEADBEEF *b.bin\r\n";
  CHECK_STR(text, expected);
  CHECK_NOT_CONTAINS(text, "1970.01.01");
}

void TestDotHashCompatibleDisabled() {
  auto options = MakeOptions();
  options.dot_hash_compatible = false;
  options.for_clipboard = true;

  const std::string expected = "DEADBEEF *dir/a.bin\r\n"
                               "DEADBEEF *b.bin\r\n";
  CHECK_STR(exportformat::BuildSumfileText(MakeTestFiles(), options, "1.2.3",
                                           k_operation_time),
            expected);
}

void TestSumfileOptionsAreHonoured() {
  auto options = MakeOptions();
  options.for_clipboard = true;
  options.dot_hash_compatible = false;
  options.uppercase = false;
  options.double_space = true;
  options.forward_slashes = false;
  options.unix_endings = false; // clipboard always uses CRLF

  CHECK_STR(exportformat::BuildSumfileText(MakeTestFiles(), options, "1.2.3",
                                           k_operation_time),
            "deadbeef  dir\\a.bin\r\n"
            "deadbeef  b.bin\r\n");
}

void TestFailedFilesAreSkipped() {
  auto files = MakeTestFiles();
  files[0].error = true;

  auto options = MakeOptions();
  options.for_clipboard = true;
  options.dot_hash_compatible = false;

  CHECK_STR(
      exportformat::BuildSumfileText(files, options, "1.2.3", k_operation_time),
      "DEADBEEF *b.bin\r\n");
}

void TestSfvTimestamps() {
  auto options = MakeOptions();
  options.algorithm = 0; // CRC32 stands in for the MD5 bytes here
  options.banner_date = true;
  options.uppercase = false;

  const auto text = exportformat::BuildSfvText(MakeTestFiles(), options,
                                               "1.2.3", k_operation_time);
  const std::string expected =
      "; Generated by OpenHashTab 1.2.3 at 2026-11-23T16:33:00Z\n"
      "; https://github.com/namazso/OpenHashTab/\n"
      ";\n"
      "dir/a.bin 0123456789abcdef\n"
      "b.bin 0123456789abcdef\n";
  CHECK_STR(text, expected);
}

struct TestCase {
  const char *name;
  void (*fn)();
};

} // namespace

int main() {
  ForceUtcTimezone();

  // The timestamp assertions assume local time == UTC; fail loudly instead of
  // silently testing something else
  std::tm utc_source{};
  if (!exportformat::ToLocalTm(k_source_time, utc_source) ||
      utc_source.tm_hour != 7 || utc_source.tm_mday != 8) {
    std::printf("FAIL: could not force UTC as the local timezone, local time "
                "tests would be meaningless\n");
    return 1;
  }

  const TestCase tests[] = {
      {"FileTimeConversion", TestFileTimeConversion},
      {"TimeFormatting", TestTimeFormatting},
      {"DotHashTimestampSelection", TestDotHashTimestampSelection},
      {"ExportToFileTimestamps", TestExportToFileTimestamps},
      {"ClipboardTimestamps", TestClipboardTimestamps},
      {"OperationTimeFallback", TestOperationTimeFallback},
      {"DotHashExporterTimestamps", TestDotHashExporterTimestamps},
      {"DotHashCompatibleDisabled", TestDotHashCompatibleDisabled},
      {"SumfileOptionsAreHonoured", TestSumfileOptionsAreHonoured},
      {"FailedFilesAreSkipped", TestFailedFilesAreSkipped},
      {"SfvTimestamps", TestSfvTimestamps},
  };

  for (const auto &test : tests) {
    const auto before = g_failures;
    test.fn();
    std::printf("%s %s\n", g_failures == before ? "ok  " : "FAIL", test.name);
  }

  std::printf("\n%d checks, %d failures\n", g_checks, g_failures);

  if (g_failures != 0) {
    std::printf("RESULT: FAILED\n");
    return 1;
  }

  std::printf("RESULT: PASSED\n");
  return 0;
}
