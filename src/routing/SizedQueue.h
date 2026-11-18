//wrap std::queue with push and pop functions that enforce max size, max size should bebadded to cdp cfg
#pragma once

#include "../utils/DuckLogger.h"
#include <queue>
#include <algorithm>
#include <optional>
#include <utility>
#include "../CdpPacket.h"

class SizedQueue{
  public:
  SizedQueue(size_t maxSize = CDPCFG_MAX_QUEUE_SIZE): maxSize(maxSize) {}
  ~SizedQueue() = default; 

  bool enqueue(const CdpPacket& packet){
    if (packetQueue.size() < maxSize){
      packetQueue.push(packet);
      ++accepted;
      highWater = std::max(highWater, packetQueue.size());
    }else{
      loginfo_ln("[ROUTER] packet queue max size exceeded");
      ++dropped;
      return false;
    }
    logdbg_ln("[ROUTER] queue size: %zu", packetQueue.size());
    return true;
  }

  std::optional<CdpPacket> dequeue(){
    if(!packetQueue.empty()){
      CdpPacket packet = packetQueue.front();
      packetQueue.pop();
      return packet;
    } else{
      return std::nullopt;
    }
  }

  void clear(){
    cleared += packetQueue.size();
    packetQueue = std::queue<CdpPacket>{};
  }

  // Remove selected pending packets while retaining FIFO order and admission
  // statistics. Intentional removals count as cleared, not overflow drops.
  template <typename Predicate>
  size_t removeIf(Predicate predicate) {
    const size_t originalSize = packetQueue.size();
    size_t removed = 0;
    for (size_t i = 0; i < originalSize; ++i) {
      CdpPacket packet = std::move(packetQueue.front());
      packetQueue.pop();
      if (predicate(packet)) ++removed;
      else packetQueue.push(std::move(packet));
    }
    cleared += removed;
    return removed;
  }

  const CdpPacket* front() const { return packetQueue.empty() ? nullptr : &packetQueue.front(); }
  size_t size() const { return packetQueue.size(); }
  size_t highWater = 0;
  uint32_t accepted = 0, dropped = 0, cleared = 0;
  
private:
  size_t maxSize;
  std::queue<CdpPacket> packetQueue;
};
