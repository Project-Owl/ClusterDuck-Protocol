#ifndef PAPADUCK_H
#define PAPADUCK_H

#include "Duck.h"
#include "../wifi/DuckWifi.h"

template <typename WifiCapability = DuckWifi, typename RadioType = DuckLoRa>
class PapaDuck : public Duck<WifiCapability, RadioType> {
public:
  using Duck<WifiCapability, RadioType>::Duck;
  
  PapaDuck(std::string name = "PAPADUCK") : Duck<WifiCapability, RadioType>(std::move(name)) {}
  ~PapaDuck() {}

  /// Papa Duck callback functions signature.
  using rxDoneCallback = void (*)(CdpPacket data);
  using txDoneCallback = void (*)(void);
  /**
   * @brief Register callback for handling data received from duck devices
   * 
   * The callback will be invoked if the packet needs to be relayed (i.e not seen before)
   * @param cb a callback to handle data received by the papa duck
   */
  void onReceiveDuckData(rxDoneCallback cb) { this->recvDataCallback = cb; }

  /**
   * @brief Get the DuckType
   *
   * @returns the duck type defined as DuckType
   */
  DuckType getType() { return DuckType::PAPA; }

  //remove this when mqtt quack pack is added
  bool isWifiConnected(){
    return this->duckWifi.connected();
  }

  bool muidNotReceived(Muid muid){
   return this->router.getFilter().bloom_check(muid.data(), MUID_LENGTH) == 0;
  }

protected:
  /**
   * @brief Hand the PapaDuck's own health message to the receive callback.
   *
   * The PapaDuck is where health messages go, so sending its own over LoRa
   * would only use airtime. The callback gets it as a packet from this duck to
   * PAPADUCK_DUID with hop count 0, the same way it gets health from the mesh,
   * so the application can publish it to the cloud.
   * @returns DUCK_ERR_NONE if delivered, DUCK_ERR_NOT_SUPPORTED without a
   * callback, DUCKPACKET_ERR_SIZE_INVALID for an empty or oversized message.
   */
  int reportHealth(const std::string& message) override {
    if (!recvDataCallback) return DUCK_ERR_NOT_SUPPORTED;
    CdpPacket packet(PAPADUCK_DUID, topics::health,
      std::vector<uint8_t>(message.begin(), message.end()), this->duid, getType());
    this->router.getFilter().assignUniqueMessageId(packet);
    const int err = packet.prepareForSending();
    if (err != DUCK_ERR_NONE) return err;
    CdpPacket delivered(packet.asBytes());
    delivered.timeReceived = millis();
    recvDataCallback(delivered);
    return DUCK_ERR_NONE;
  }

private:
  rxDoneCallback recvDataCallback = nullptr;
  
  void handleReceivedPacket(CdpPacket rxPacket) {
    loginfo_ln("====> handleReceivedPacket: START");
    
    if (recvDataCallback) recvDataCallback(rxPacket);

    //Check if Duck is desitination for this packet before relaying
    if (duckutils::isEqual(BROADCAST_DUID, rxPacket.dduid)) {
        ifBroadcast(rxPacket);
    } else if(duckutils::isEqual(this->duid, rxPacket.dduid) || duckutils::isEqual(rxPacket.dduid, PAPADUCK_DUID)) { //Target device check
        ifNotBroadcast(rxPacket);
    } else { //If it's meant for a specific target but not this one
        ifNotBroadcast(rxPacket, true);
    }
    this->router.getFilter().bloom_add(rxPacket.muid.data(), MUID_LENGTH);
  } 

  void ifBroadcast(CdpPacket rxPacket) { 
    int err;
    switch(rxPacket.topic) {
        case reservedTopic::ping:
            loginfo_ln("PING received. Sending PONG!");
            err = this->sendPong();
            if (err != DUCK_ERR_NONE) {
                logerr_ln("ERROR failed to send pong message. rc = %d",err);
            }
            break;
        case reservedTopic::pong:
            loginfo_ln("PONG received. Ignoring!");
            break;
        // case reservedTopic::cmd:
        //     loginfo_ln("Command received");

        //     err = this->broadcastPacket(rxPacket);
            
        //     if (err != DUCK_ERR_NONE) {
        //         logerr_ln("====> ERROR handleReceivedPacket failed to relay. rc = %d",err);
        //     } else {
        //         loginfo_ln("handleReceivedPacket: packet RELAY DONE");
        //     }
        //     break;
        default:
            err = this->broadcastPacket(rxPacket);
            if (err != DUCK_ERR_NONE) {
                logerr_ln("====> ERROR handleReceivedPacket failed to relay. rc = %d",err);
            } else {
                loginfo_ln("handleReceivedPacket: packet RELAY DONE");
            }
    }
}

void ifNotBroadcast(CdpPacket rxPacket, bool relay = false) {
    int err;
    switch(rxPacket.topic) {
        case reservedTopic::ping:
            loginfo_ln("PING received. Sending PONG!");
            err = this->sendPong();
            if (err != DUCK_ERR_NONE) {
                logerr_ln("ERROR failed to send pong message. rc = %d",err);
            }
            break;
        case reservedTopic::pong:
            loginfo_ln("PONG received. Ignoring!");
            break;
        // case reservedTopic::cmd:
        //     loginfo_ln("Command received");

        //     err = this->broadcastPacket(rxPacket);
            
        //     if (err != DUCK_ERR_NONE) {
        //         logerr_ln("====> ERROR handleReceivedPacket failed to relay. rc = %d",err);
        //     } else {
        //         loginfo_ln("handleReceivedPacket: packet RELAY DONE");
        //     }
        //     break;
        default:
          if(relay){
            this->forwardPacket(rxPacket);
          }               
    }
  }

};

#endif
