#ifndef DUCKLINK_H
#define DUCKLINK_H

#include "Duck.h"

template <typename WifiCapability = DuckWifiNone, typename RadioType = DuckLoRa>
class DuckLink : public Duck<WifiCapability, RadioType> {
  public:
    using Duck<WifiCapability, RadioType>::Duck;
    
    DuckLink(std::string name = "LINK0001") : Duck<WifiCapability, RadioType>(std::move(name)) {}
    ~DuckLink() override {}
    
    /**
     * @brief Get the DuckType
     *
     * @returns the duck type defined as DuckType
     */
    DuckType getType() { return DuckType::LINK; }

    using rxDoneCallback = void (*)(CdpPacket data);
    /**
     * @brief Register callback for handling data received from duck devices
     * 
     * The callback will be invoked if the packet needs to be relayed (i.e not seen before)
     * @param cb a callback to handle data received by the papa duck
     */
    void onReceiveDuckData(rxDoneCallback cb) { this->recvDataCallback = cb; }

  private:
    rxDoneCallback recvDataCallback = nullptr;

    /**
     * @brief Handles any packets received by the duck. Overrides the pure virtual function in Duck base class.
     */
    void handleReceivedPacket(CdpPacket rxPacket) override{
        bool relay = false;
        
        loginfo_ln("====> handleReceivedPacket: START");

        if (recvDataCallback) recvDataCallback(rxPacket);
        
        //Check if Duck is desitination for this packet before relaying
        if (duckutils::isEqual(BROADCAST_DUID, rxPacket.dduid)) {
            ifBroadcast(rxPacket);
        } else if(duckutils::isEqual(this->duid, rxPacket.dduid)) { //Target device check
            ifNotBroadcast(rxPacket, false);
        } else { //If it's meant for a specific target but not this one
            ifNotBroadcast(rxPacket, true);
        }
        this->router.getFilter().bloom_add(rxPacket.muid.data(), MUID_LENGTH);
    }
  
      void ifBroadcast(CdpPacket rxPacket) {
        int err = DUCK_ERR_NONE;
          switch(rxPacket.topic) {
              case reservedTopic::ping:
                  loginfo_ln("PING received. Sending PONG!");
                  err = this->sendPong();
                  if (err != DUCK_ERR_NONE) {
                      logerr_ln("ERROR failed to send pong message. rc = %d",err);
                  }
                  break;
              case topics::cmd_tx:{
                  ArduinoJson::JsonDocument json;
                  std::string packetStr(rxPacket.data.begin(), rxPacket.data.end());
                  DeserializationError error = deserializeJson(json, packetStr);
                  if (error) {
                      logerr_ln("Duck Command cmd_tx deserialization failed: %s", error.c_str());
                      break;
                  }
                  loginfo_ln("Command received, updating transmission power");
                  
                  int txPwr = json["txPwr"];

                  if (txPwr < 14 || txPwr > 22) {
                    logerr_ln("Invalid argument -- tx power: %i", txPwr);
                    err = DUCK_ERR_INVALID_ARGUMENT;
                    break;
                  }

                  if (err == DUCK_ERR_NONE){
                    this->eeprom_preferences.putInt("txPwr", txPwr);
                    Serial.print("updating tx power to value : ");
                    Serial.println(txPwr);
                  }
                 
                  err = this->broadcastPacket(rxPacket);
                  if (err != DUCK_ERR_NONE) {
                    logerr_ln("====> ERROR handleReceivedPacket failed to relay. rc = %d",err);
                    } else {
                        loginfo_ln("handleReceivedPacket: packet RELAY DONE");
                    }
                  break;
              }
              case topics::cmd_time:{
                  loginfo_ln(" !!!!!!!!!!!!!!!!!!!Command received, updating time to match papa");
                  ArduinoJson::JsonDocument json;

                    std::string packetStr(rxPacket.data.begin(), rxPacket.data.end());
                    DeserializationError error = deserializeJson(json, packetStr);
                    if (error) {
                        logerr("JSON parse failed: ");
                        logerr(error.c_str());
                    } else{
                      uint32_t epoch = json["epoch"].as<uint32_t>();
                      this->rtc.setTime(epoch);
                      Serial.printf("RTC set to %u\n", epoch);
                    }
                  break;
              }
              default:
                  loginfo_ln("handleReceivedPacket: packet received, skipping relay.");
          }
      }
  
      void ifNotBroadcast(CdpPacket rxPacket, bool relay = false) {
        int err = DUCK_ERR_NONE;
          switch(rxPacket.topic) {
              default:
              loginfo_ln("handleReceivedPacket: packet received, skipping forward.");    
          }
      }
};

#endif
