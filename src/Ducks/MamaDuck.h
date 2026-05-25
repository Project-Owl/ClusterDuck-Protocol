#ifndef MAMADUCK_H
#define MAMADUCK_H

#include "Duck.h"
#include "../utils/MemoryFree.h"


template <typename WifiCapability = DuckWifiNone, typename RadioType = DuckLoRa>
class MamaDuck : public Duck<WifiCapability, RadioType> {
public:
    using Duck<WifiCapability, RadioType>::Duck;

    MamaDuck(std::string name = "MAMA0001") : Duck<WifiCapability, RadioType>(std::move(name)) {}

    ~MamaDuck() {};

    using rxDoneCallback = void (*)(CdpPacket data);
    /**
     * @brief Register callback for handling data received from duck devices
     * 
     * The callback is invoked for every packet the duck hears, including repeated
     * copies of the same message relayed by different ducks.
     * @param cb a callback to handle data received by the mama duck
     */
    void onReceiveDuckData(rxDoneCallback cb) { this->recvDataCallback = cb; }

    /**
     * @brief Get the DuckType
     * 
     * @returns the duck type defined as DuckType
     */
    DuckType getType() {return DuckType::MAMA;}

private :
    WifiCapability duckWifi;
    rxDoneCallback recvDataCallback = nullptr;
    /**
     * @brief Handles any packets received by the duck. Overrides the pure virtual function in Duck base class.
     * Could be a RREQ, RREP, PING, PONG or DATA packet on its associated topic.
     *
     */
    void handleReceivedPacket(CdpPacket rxPacket) override {
        logdbg_ln("====> handleReceivedPacket: START");
        if (recvDataCallback) recvDataCallback(rxPacket);

        //Check if Duck is desitination for this packet before relaying
        if (duckutils::isEqual(BROADCAST_DUID, rxPacket.dduid)) {
            ifBroadcast(rxPacket);
        } else if(duckutils::isEqual(this->duid, rxPacket.dduid)) { //Target device check
            ifNotBroadcast(rxPacket);
        } else { //If it's meant for a specific target but not this one
            ifNotBroadcast(rxPacket, true);
        }
        this->router.getFilter().bloom_add(rxPacket.muid.data(), MUID_LENGTH);
    }

    /**
     * @brief Handles packets that are meant for broadcasing (i.e RREQ, PING, CMD, etc).
     * Overrides the pure virtual function in Duck base class.
     * Only responds to neighbors to decrease strain on the network.
     * @param rxPacket the received packet to be processed by the mama duck if it's a broadcast packet (RREQ, PING, CMD, etc).
     * If it's a RREQ with hop count > 0, it will send a response.
     * If it's a PING, a PONG will be sent back. If it's a CMD, it will be relayed.
     */
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
            case reservedTopic::pong:
                loginfo_ln("PONG received. Ignoring!");
                break;
#if CDPCFG_REMOTE_TX_POWER
            case topics::cmd_tx: {
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
                    Serial.print("updating tx power to value : ");
                    Serial.println(txPwr);
                    this->eeprom_preferences.putInt("txPwr", txPwr);
                  }
                 
                  err = this->broadcastPacket(rxPacket);
                  if (err != DUCK_ERR_NONE) {
                    logerr_ln("====> ERROR handleReceivedPacket failed to relay. rc = %d",err);
                    } else {
                        loginfo_ln("handleReceivedPacket: packet RELAY DONE");
                    }
                  break;
            }
#endif
#if CDPCFG_REMOTE_TIME_SYNC
            case topics::cmd_time:
                // Must run before the relay below, which marks the message as seen.
                // Relayed whether or not it was applied, like any other packet.
                this->applyTimeCommand(rxPacket);
                [[fallthrough]];
#endif
            default:
                err = this->broadcastPacket(rxPacket);
                if (err != DUCK_ERR_NONE) {
                    logerr_ln("====> ERROR handleReceivedPacket failed to relay. rc = %d",err);
                } else {
                    loginfo_ln("handleReceivedPacket: packet RELAY DONE");
                }
        }
    }

    /**
     * @brief Handles packets that are meant for specific targets (i.e RREP, PONG, etc).
     * If the RREQ is not meant for the mama duck but is still a directed packet, it will be relayed; otherwise, sends RREP.
     * If the packet is a RREP meant for the mama duck, it will update the routing table with the path information; otherwise, it will be relayed to the next hop.
     * Overrides the pure virtual function in Duck base class.
     * @param rxPacket
     * @param relay
     */
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
#if CDPCFG_REMOTE_TX_POWER
            case topics::cmd_tx: {
                // A command for another duck is passed on, not applied here.
                if (relay) {
                    this->forwardPacket(rxPacket);
                    break;
                }
                // Targeted cmd_tx used to log and do nothing, so a command aimed at a
                // single duck was silently dropped. Apply the same update as the
                // broadcast case; deliberately no relay, since this one is addressed
                // to us.
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
                this->eeprom_preferences.putInt("txPwr", txPwr);
                loginfo_ln("updating tx power to value : %i", txPwr);
                break;
            }
#endif
#if CDPCFG_REMOTE_TIME_SYNC
            case topics::cmd_time:
                // Addressed to us: apply it, no relay. Otherwise pass it on.
                if (relay) this->forwardPacket(rxPacket);
                else this->applyTimeCommand(rxPacket);
                break;
#endif
            default:
                if(relay){
                    this->forwardPacket(rxPacket); 
                }       
        }
    }

};

#endif
