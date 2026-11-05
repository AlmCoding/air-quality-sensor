#pragma once

#include <Arduino.h>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace aqlog {

inline constexpr uint16_t kMagic = 0xA5C3;
inline constexpr uint8_t kVersion = 0x02;
inline constexpr size_t kEntriesPerBlock = 12;
inline constexpr size_t kBlockSize =
    8 + 12 * 7; // 8 bytes for header + 12 entries * 7 bytes each
inline constexpr uint8_t kWindowOpen = 0x01;

#pragma pack(push, 1)
struct Entry {
  uint16_t co2_ppm;
  int16_t temp_c_x100;
  uint16_t rh_pct_x100;
  uint8_t flags;
};
#pragma pack(pop)

struct Block {
  uint16_t magic = kMagic;
  uint16_t crc16 = 0;
  uint32_t timestamp = 0;
  Entry entries[kEntriesPerBlock];
};

static_assert(sizeof(Entry) == 7, "Entry must be 7 bytes");
static_assert(kBlockSize == 8 + kEntriesPerBlock * 7, "block size mismatch");

} // namespace aqlog

struct DataStoreFile {
  String name;
  bool isDirectory;
};

class DataStore {
public:
  static DataStore &instance();

  bool setFileName(const char *fileName);
  bool listFiles(const char *path, std::vector<DataStoreFile> &files);
  bool addSample(uint16_t co2Ppm, float tempC, float rhPct, bool windowOpen,
                 uint32_t unixTimestamp);
  size_t sampleCount() const;
  bool isInErrorState() const;

  DataStore(const DataStore &) = delete;
  DataStore &operator=(const DataStore &) = delete;

private:
  DataStore() = default;

  bool writeBlock();
  bool restoreFileToSize(size_t size);
  bool ensureFileSystemMounted();

  static constexpr size_t kMaxFileNameLength = 32;
  static constexpr size_t kCrcOffset = 2;

  aqlog::Block block_{};
  size_t sampleCount_ = 0;
  char fileName_[kMaxFileNameLength + 1] = {};
  bool fileSystemMounted_ = false;
  bool errorState_ = false;
};
