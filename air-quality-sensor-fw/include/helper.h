#pragma once

#include <cstddef>
#include <cstdint>

namespace helper {

bool isCStringEmpty(const char *value);
bool copyCString(const char *source, char *destination, size_t destinationSize);
bool unixTimestampToHumanReadable(uint32_t unixTimestamp, char *destination,
                                  size_t destinationSize);

void writeUint16LittleEndian(uint8_t *destination, uint16_t value);
void writeUint32LittleEndian(uint8_t *destination, uint32_t value);
uint16_t calculateCrc16(const uint8_t *data, size_t length);

} // namespace helper
