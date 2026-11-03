#pragma once

#include <atomic>
#include <cstdint>
#include <ctime>
#include <sys/time.h>

class Connectivity {
public:
  static Connectivity &instance();

  bool init();
  void run();

private:
  Connectivity() = default;
  Connectivity(const Connectivity &) = delete;
  Connectivity &operator=(const Connectivity &) = delete;

  static void onTimeSync(struct timeval *time);
  bool syncTime();

  std::atomic_bool timeSyncReceived_{false};
  std::time_t nextSyncTime_ = 0;
  bool wifiEventHandlerRegistered_ = false;
  bool initialized_ = false;
};
