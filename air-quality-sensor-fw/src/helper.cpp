#include "helper.h"

#include <cstring>

namespace helper {

bool isCStringEmpty(const char *value) {
  return value == nullptr || value[0] == '\0';
}

bool copyCString(const char *source, char *destination,
                 size_t destinationSize) {
  if (destination == nullptr || source == nullptr || destinationSize == 0) {
    return false;
  }

  size_t length = 0;
  while (length < destinationSize && source[length] != '\0') {
    ++length;
  }
  if (length == destinationSize) {
    return false;
  }

  std::memcpy(destination, source, length + 1);
  return true;
}

void writeUint16LittleEndian(uint8_t *destination, uint16_t value) {
  destination[0] = static_cast<uint8_t>(value & 0xFF);
  destination[1] = static_cast<uint8_t>(value >> 8);
}

// CRC-16/CCITT-FALSE.
uint16_t calculateCrc16(const uint8_t *data, size_t length) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < length; ++i) {
    crc ^= static_cast<uint16_t>(data[i]) << 8;
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc & 0x8000) != 0 ? static_cast<uint16_t>((crc << 1) ^ 0x1021)
                                : static_cast<uint16_t>(crc << 1);
    }
  }
  return crc;
}

} // namespace helper
