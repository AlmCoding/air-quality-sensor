#include "DataStore.h"
#include "common.h"

#include "Clock.h"
#include "helper.h"

#include <LittleFS.h>

#include <cassert>
#include <cmath>
#include <cstdio>

#define DEBUG_ENABLE_DATASTORE 1
#if ((DEBUG_ENABLE_DATASTORE == 1) && (ENABLE_DEBUG_OUTPUT == 1))
#define DEBUG_INFO(f, ...) debugPrint("[INF][DataStore]", f, ##__VA_ARGS__)
#define DEBUG_ERROR(f, ...) debugPrint("[ERR][DataStore]", f, ##__VA_ARGS__)
#else
#define DEBUG_INFO(...)
#define DEBUG_ERROR(...)
#endif

DataStore &DataStore::instance() {
  static DataStore dataStore;
  return dataStore;
}

bool DataStore::setFileName(const char *fileName) {
  if (helper::isCStringEmpty(fileName)) {
    return false;
  }

  return helper::copyCString(fileName, fileName_, sizeof(fileName_));
}

bool DataStore::addSample(uint16_t co2Ppm, float tempC, float rhPct,
                          bool windowOpen) {
  assert(sampleCount_ < aqlog::kEntriesPerBlock);

  if (errorState_) {
    return false;
  }

  if (helper::isCStringEmpty(fileName_)) {
    DEBUG_ERROR("DataStore file name is not set!");
    return false;
  }

  if (!std::isfinite(tempC) || !std::isfinite(rhPct)) {
    DEBUG_ERROR("Invalid sample values: CO2=%u ppm, Temp=%.2f C, RH=%.2f %%", //
                co2Ppm, tempC, rhPct);
    return false;
  }

  const float tempC_x100 = tempC * 100.0f;
  const float rhPct_x100 = rhPct * 100.0f;

  if (sampleCount_ == 0) {
    uint32_t timestamp = 0;
    if (!Clock::instance().isSet() ||
        !Clock::instance().getUnixTime(timestamp)) {
      DEBUG_ERROR("Failed to get Unix time from Clock!");
      return false;
    }
    block_.timestamp = timestamp;
  }

  aqlog::Entry &entry = block_.entries[sampleCount_];
  entry.co2_ppm = co2Ppm;
  entry.temp_c_x100 = static_cast<int16_t>(tempC_x100);
  entry.rh_pct_x100 = static_cast<uint16_t>(rhPct_x100);
  entry.flags = windowOpen ? aqlog::kWindowOpen : 0;
  ++sampleCount_;

  if (sampleCount_ < aqlog::kEntriesPerBlock) {
    DEBUG_INFO("Sample(%u/%u) added: CO2=%u, Temp=%d, RH=%u, WindowOpen=%s", //
               sampleCount_, aqlog::kEntriesPerBlock,                        //
               co2Ppm, entry.temp_c_x100, entry.rh_pct_x100,
               windowOpen ? "true" : "false");
    return true;
  }

  if (!writeBlock()) {
    block_ = aqlog::Block{};
    sampleCount_ = 0;
    return false;
  }

  block_ = aqlog::Block{};
  sampleCount_ = 0;
  return true;
}

size_t DataStore::sampleCount() const { return sampleCount_; }

bool DataStore::isInErrorState() const { return errorState_; }

bool DataStore::writeBlock() {
  if (helper::isCStringEmpty(fileName_) || !ensureFileSystemMounted()) {
    return false;
  }

  uint8_t serialized[aqlog::kBlockSize] = {};
  helper::writeUint16LittleEndian(serialized, block_.magic);
  helper::writeUint16LittleEndian(serialized + 4, block_.timestamp);

  size_t offset = 8;
  for (const aqlog::Entry &entry : block_.entries) {
    helper::writeUint16LittleEndian(serialized + offset, entry.co2_ppm);
    helper::writeUint16LittleEndian(serialized + offset + 2,
                                    static_cast<uint16_t>(entry.temp_c_x100));
    helper::writeUint16LittleEndian(serialized + offset + 4, entry.rh_pct_x100);
    serialized[offset + 6] = entry.flags;
    offset += 7;
  }

  // The zero-initialized CRC field remains zero during calculation.
  const uint16_t crc = helper::calculateCrc16(serialized, sizeof(serialized));
  helper::writeUint16LittleEndian(serialized + kCrcOffset, crc);

  File file = LittleFS.open(fileName_, FILE_APPEND);
  if (!file) {
    DEBUG_ERROR("Failed to open file for APPEND: %s", fileName_);
    errorState_ = true;
    return false;
  }

  const size_t originalSize = file.size();
  const size_t bytesWritten = file.write(serialized, sizeof(serialized));
  file.flush();
  file.close();
  if (bytesWritten != sizeof(serialized)) {
    DEBUG_ERROR("Failed to write block to file: %s", fileName_);
    DEBUG_INFO("Expected to write %u bytes, but only wrote %u bytes", //
               sizeof(serialized), bytesWritten);

    if (!restoreFileToSize(originalSize)) {
      DEBUG_ERROR("Failed to restore file to original size: %s", fileName_);
      DEBUG_INFO("Entering error state to prevent further writes");
      errorState_ = true;
    }
    return false;
  }

  return true;
}

bool DataStore::restoreFileToSize(size_t size) {
  char temporaryFileName[kMaxFileNameLength + sizeof(".rollback")] = {};
  const int nameLength = std::snprintf(
      temporaryFileName, sizeof(temporaryFileName), "%s.rollback", fileName_);
  if (nameLength < 0 ||
      static_cast<size_t>(nameLength) >= sizeof(temporaryFileName)) {
    return false;
  }

  if (LittleFS.exists(temporaryFileName) &&
      !LittleFS.remove(temporaryFileName)) {
    DEBUG_ERROR("Failed to remove temporary file: %s", temporaryFileName);
    return false;
  }

  File source = LittleFS.open(fileName_, FILE_READ);
  if (!source) {
    DEBUG_ERROR("Failed to open file for READ: %s", fileName_);
    return false;
  }

  File temporary = LittleFS.open(temporaryFileName, FILE_WRITE);
  if (!temporary) {
    DEBUG_ERROR("Failed to open file for WRITE: %s", temporaryFileName);
    source.close();
    return false;
  }

  uint8_t buffer[256];
  size_t remaining = size;
  bool copied = true;
  while (remaining > 0) {
    const size_t bytesToCopy =
        remaining < sizeof(buffer) ? remaining : sizeof(buffer);
    const size_t bytesRead = source.read(buffer, bytesToCopy);
    if (bytesRead != bytesToCopy ||
        temporary.write(buffer, bytesRead) != bytesRead) {
      copied = false;
      break;
    }
    remaining -= bytesRead;
  }

  temporary.flush();
  copied = copied && temporary.size() == size;
  source.close();
  temporary.close();

  if (!copied) {
    DEBUG_ERROR("Failed to copy original file to temporary file: %s", //
                temporaryFileName);
    LittleFS.remove(temporaryFileName);
    return false;
  }

  // Arduino's LittleFS File API has no truncate operation, so replace the
  // damaged file with a copy containing only its original bytes.
  if (!LittleFS.rename(temporaryFileName, fileName_)) {
    DEBUG_ERROR("Failed to rename temporary file: %s", temporaryFileName);
    LittleFS.remove(temporaryFileName);
    return false;
  }

  return true;
}

bool DataStore::ensureFileSystemMounted() {
  if (fileSystemMounted_) {
    return true;
  }

  if (!LittleFS.begin(false)) {
    DEBUG_ERROR("Failed to mount LittleFS file system!");
    return false;
  }

  fileSystemMounted_ = true;
  return true;
}
