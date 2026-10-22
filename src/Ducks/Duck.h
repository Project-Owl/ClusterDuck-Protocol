#ifndef DUCK_H
#define DUCK_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <arduino-timer.h>
#include <Preferences.h>
#include "../utils/DuckError.h"
#include "../include/cdpcfg.h"
#include "../radio/DuckLoRa.h"
#include "DuckTypes.h"
#include "../utils/DuckUtils.h"
#include <cassert>
#include "../CdpPacket.h"
#include "../DuckEsp.h"
#include "../wifi/DuckWifiNone.h"
#include "../routing/DuckRouter.h"
#include "../routing/RouteJSON.h"
#include "../routing/SizedQueue.h"
#include "../utils/MemoryFree.h"
#include <ESP32Time.h>
#include <esp_system.h>
#include <algorithm>

//templated class to require some radio capability
template <typename WifiCapability = DuckWifiNone, typename RadioType = DuckLoRa>
class Duck {
  public:
    virtual ~Duck(){
    };
    Preferences eeprom_preferences;
    ESP32Time rtc; 
    /**
     * @brief Duck main running loop
     */
    void run(){
      duckRadio.serviceInterruptFlags();
      Duck::logIfLowMemory();
      if (!meshReady) return;
      // Membership is local readiness, never permission to exchange mesh data.
      // Gateway reachability is reported independently by the router.
      if (duckRadio.getReceiveFlag()) queueReceivedPacket();
      if (auto packet = rxQueue.dequeue()) processReceivedPacket(*packet);
      if (initialDiscoveryPending && uint32_t(millis() - setupAt) >= initialDiscoveryDelay) {
        initialDiscoveryPending = false;
        discover(BROADCAST_DUID, 1); // startup, one-hop neighbour discovery
      }
      // Do not dequeue until pacing allows an attempt. Alternate control/data
      // when both are pending, so route discovery cannot starve detections.
      if (uint32_t(millis() - lastPacketTx) >= CDPCFG_MAX_PACKET_SEND_RATE) {
        const bool control = reqQueue.size() && (preferControl || !txQueue.size());
        SizedQueue& queue = control ? reqQueue : txQueue;
        if (const CdpPacket* front = queue.front()) {
          CdpPacket packet = *front;
          lastPacketTx = millis();
          const int err = sendToRadio(packet);
          if (err == DUCKLORA_ERR_TX_BUSY) ++radioBusy;
          else {
            queue.dequeue();
            preferControl = !control;
            if (err == DUCK_ERR_NONE) ++radioStarted;
            else ++radioFailed;
          }
        }
      }
      duckTimer.tick();
    }

    int setupWithDefaults() {
      this->setupSerial(115200);
      if (!eeprom_preferences.begin("duck")) {
        logerr_ln("Failed to initialise EEPROM, aborting duck setup");
        return DUCK_ERR_EEPROM_INIT;
      }

      // Seed the Teensy battery sleep thresholds only if they have never been set.
      if (!eeprom_preferences.isKey("teensy_off")) {
        eeprom_preferences.putFloat("teensy_off", BAT_TEENSY_POWER_OFF_V);
      }
      if (!eeprom_preferences.isKey("teensy_on")) {
        eeprom_preferences.putFloat("teensy_on", BAT_TEENSY_POWER_ON_V);
      }

      int txPwr = eeprom_preferences.getInt("txPwr", CDPCFG_RF_LORA_TXPOW);
      if (txPwr < 14 || txPwr > 22) {
          txPwr = CDPCFG_RF_LORA_TXPOW;
      }
      Serial.print("tx power value on startup : ");
      Serial.println(txPwr);
      //should i close the eeprom_preferences here??

      //lora config for use in phoenix
      const LoRaConfigParams radioParam = {
        /* band     = */ CDPCFG_RF_LORA_FREQ,
        /* txPower  = */ static_cast<int8_t>(txPwr),
        /* bw       = */ CDPCFG_RF_LORA_BW,
        /* sf       = */ CDPCFG_RF_LORA_SF,
        /* gain     = */ CDPCFG_RF_LORA_GAIN,
        /* func     = */ duckRadio.onInterrupt
    };

      int err = this->setupLoRaRadio(radioParam);
      if (err != DUCK_ERR_NONE) {
        logerr_ln("ERROR setupWithDefaults rc = %d",err); 
      } else{
        duckTimer.every(HEALTH_INTERVAL, sendHealth, this);
        duckTimer.every(SIGNAL_INTERVAL, sendSignalData, this);
        duckTimer.every(CDPCFG_ROUTE_KEEPALIVE_MS, sendRouteKeepalive, this);
        duckTimer.every(CDPCFG_DIAGNOSTIC_INTERVAL_MS, sendRoutingDiagnostics, this);
        meshReady = true;
        setupAt = millis();
        bootEpoch = esp_random();
        initialDiscoveryDelay = 2000UL + esp_random() % 4000UL;
        initialDiscoveryPending = getType() != DuckType::PAPA && getType() != DuckType::DETECTOR;
        router.setNetworkState(NetworkState::PUBLIC);
        loginfo_ln("[ROUTE] open mesh ready; protocol=2 epoch=%lu", (unsigned long)bootEpoch);
      }
      return err;
    }

    /**
     * @brief Send data to the CDP network mesh
     * @param topic the message topic
     * @param data a std::string representing the data to send
     * @param targetDevice the device UID to receive the message (default is all papa devices)
     * @return DUCK_ERR_NONE only if accepted into the software TX queue.
     */
    int sendData(uint8_t topic, const std::string data, const Duid targetDevice = PAPADUCK_DUID) {
      return sendData(topic, reinterpret_cast<const uint8_t*>(data.data()), data.size(), targetDevice);
    }

    /**
     * @brief Send data to the CDP network mesh
     * @param topic the message topic
     * @param data a vector of bytes representing the data to send
     * @param targetDevice the device UID to receive the message (default is all papa devices)
     * @return DUCK_ERR_NONE only if accepted into the software TX queue.
    */
    int sendData(uint8_t topic, const uint8_t* data, int length, const Duid targetDevice = PAPADUCK_DUID) {
      if (topic < reservedTopic::max_reserved) return DUCKPACKET_ERR_TOPIC_INVALID;
      if (!data || length <= 0 || size_t(length) > MAX_DATA_LENGTH) return DUCKPACKET_ERR_SIZE_INVALID;
      if (!meshReady) return DUCK_ERR_NOT_SUPPORTED;
      CdpPacket packet(targetDevice, topic, std::vector<uint8_t>(data, data + length), duid, getType());
      router.getFilter().assignUniqueMessageId(packet);
      if (!txQueue.enqueue(packet)) return DUCK_ERR_QUEUE_FULL;
      ++appAccepted;
      loginfo_ln("[CDP_DIAG] ACCEPT src=%s mid=%s topic=%u bytes=%u stage=queued",
        duckutils::duidToString(duid).c_str(), duckutils::arrayToHexString(packet.muid).c_str(),
        topic, (unsigned)packet.data.size());
      // Route discovery is advisory for this open mesh. Throttling it must never
      // suppress an application packet or pretend an enqueue succeeded.
      if (targetDevice != BROADCAST_DUID && getType() != DuckType::PAPA &&
          !router.getBestNextHop(targetDevice) &&
          (!hasRequestedRoute || uint32_t(millis() - lastRreqTime) >= 30000UL))
        discover(targetDevice, 2); // missing destination
      return DUCK_ERR_NONE;
    }

    /**
     * @brief sendData that allows sending for reserved topic ping
     * @returns DUCK_ERR_NONE if the data was sent successfully, an error code otherwise.
     */
    int sendPing(Duid targetDevice = BROADCAST_DUID){
      std::vector<uint8_t> data(1, 0);
      int err = sendReservedTopicData(targetDevice, reservedTopic::ping, data);
      if (err != DUCK_ERR_NONE){
        logerr_ln("ERR: failed to send ping");
      }
      return err;
    }

    /**
     * @brief Get the duck's unique ID.
     * 
     * @returns A byte vector representing the duck's unique ID
     */ 
    std::vector<uint8_t> getDuckId() {
      return std::vector<uint8_t>(duid.begin(), duid.end());
    }

    /**
     * @brief Get the duck type.
     * @returns A value representing a DuckType
     */
    virtual DuckType getType() = 0;

    // Versioned, independently bounded health pages. All counters reset at boot;
    // b identifies that epoch, u is uptime in seconds. null means unavailable.
    std::string routingDiagnostics(uint8_t page) {
      JsonDocument doc;
      doc["v"] = 2;
      doc["p"] = page;
      doc["b"] = bootEpoch;
      doc["u"] = uint32_t(millis()) / 1000;
      if (page == 0) {
        doc["C"] = std::to_string(counter);
        doc["FM"] = std::to_string(freeMemory());
        doc["r"] = int(esp_reset_reason());
        doc["n"] = int(router.getNetworkState());
        const auto hop = router.getBestNextHop(PAPADUCK_DUID);
        const auto age = router.getRouteAge(PAPADUCK_DUID);
        if (getType() == DuckType::PAPA) { doc["gw"] = "self"; doc["ga"] = 0; }
        else if (hop && age) { doc["gw"] = duckutils::duidToString(*hop); doc["ga"] = *age / 1000; }
        else { doc["gw"] = nullptr; doc["ga"] = nullptr; }
        doc["nb"] = router.directNeighborCount();
        doc["j"] = discoveryAttempts;
        doc["jr"] = discoveryReason;
        doc["nt"] = router.transitions;
      } else if (page == 1) {
        auto q = doc["q"].to<JsonArray>();
        q.add(rxQueue.size()); q.add(txQueue.size()); q.add(reqQueue.size());
        auto h = doc["h"].to<JsonArray>();
        h.add(rxQueue.highWater); h.add(txQueue.highWater); h.add(reqQueue.highWater);
        auto d = doc["d"].to<JsonArray>();
        d.add(rxQueue.dropped); d.add(txQueue.dropped); d.add(reqQueue.dropped);
      } else if (page == 2) {
        auto rq = doc["rq"].to<JsonArray>(); for (auto n : requestStats) rq.add(n);
        auto rp = doc["rp"].to<JsonArray>(); for (auto n : responseStats) rp.add(n);
        auto dr = doc["dr"].to<JsonArray>();
        dr.add(controlMalformed); dr.add(hopDropped); dr.add(controlLoopDropped);
      } else if (page == 3) {
        auto tx = doc["tx"].to<JsonArray>();
        tx.add(radioStarted); tx.add(duckRadio.getTxDoneCount()); tx.add(radioFailed);
        tx.add(radioBusy); tx.add(duckRadio.getTxTimeoutCount());
        auto app = doc["app"].to<JsonArray>();
        app.add(appAccepted); app.add(dataForwarded); app.add(dataSuppressed);
        if (router.signalSnapshot) {
          auto signal = doc["sg"].to<JsonArray>();
          signal.add((*router.signalSnapshot)[0]); signal.add((*router.signalSnapshot)[1]);
          signal.add(uint32_t(millis() - (*router.signalSnapshot)[2]) / 1000);
        } else doc["sg"] = nullptr;
        doc["ne"] = router.neighborEvictions;
      }
      std::string result;
      serializeJson(doc, result);
      // Never silently hand an oversized diagnostic packet to the radio.
      if (result.size() > MAX_DATA_LENGTH) return "";
      return result;
    }

    void joinWifiNetwork(std::string ssid = "", std::string password = ""){
      int err = this->duckWifi.joinNetwork(ssid, password);

      // If we fail to connect to WiFi, retry a few times
      if (err == DUCK_INTERNET_ERR_CONNECT) {
        int retry=0;
        while ( err ==  DUCK_INTERNET_ERR_CONNECT && retry < 5 ) {
          Serial.printf("WiFi connection failed, retry connection: %s\n", ssid.c_str());
          delay(5000);
          err = err = this->duckWifi.joinNetwork(ssid, password);
          retry++;
        }  
      }

        if (err == DUCK_INTERNET_ERR_CONNECT) {
          logerr_ln("ERROR wifi setup failed = %d",err);
        }
    }


  protected:
    /**
     * @brief Construct a new Duck object.
     *
     */
    Duck(std::string name){ //check this for correct length
      duid.fill(0);
      std::copy_n(name.begin(), std::min(name.size(), duid.size()), duid.begin());
    }

    RadioType duckRadio;
    WifiCapability duckWifi;
    static constexpr int MEMORY_LOW_THRESHOLD = PACKET_LENGTH + sizeof(CdpPacket);
    Duid duid;
    DuckRouter router;


    
    /**
     * @brief Duck-type specific handler for different packet topics
     */ 
    virtual void handleReceivedPacket(CdpPacket packet) = 0;

    int broadcastPacket(CdpPacket& packet){
      if (packet.hopCount >= MAX_HOPS) { ++hopDropped; return DUCKPACKET_ERR_MAX_HOPS; }
      if (router.getFilter().bloom_check(packet.muid.data(), MUID_LENGTH)) {
        ++dataSuppressed;
        return DUCK_ERR_NONE;
      }
      CdpPacket relay = packet;
      ++relay.hopCount;
      if (!txQueue.enqueue(relay)) return DUCK_ERR_QUEUE_FULL;
      router.getFilter().bloom_add(packet.muid.data(), MUID_LENGTH);
      ++dataForwarded;
      return DUCK_ERR_NONE;
    }

    int forwardPacket(CdpPacket& packet){
      // Bounded duplicate-suppressed forwarding also works without a known
      // destination route, including after sleep or late gateway arrival.
      return broadcastPacket(packet);
    }

    unsigned long lastRreqTime = 0L;
    unsigned long lastPacketTx = 0L;

    /**
     * @brief Set up USB serial port
     * @param baudRate baud rate  matching serial monitor
     * @returns DUCK_ERR_NONE if the data was sent successfully, an error code otherwise.
     */ 
    int setupSerial(int baudRate) {
      // // This gives us 10 seconds to do a hard reset if the board is in a bad state after power cycle
      // while (!Serial && millis() < 10000);
    
      Serial.begin(baudRate);
      loginfo_ln("setupSerial rc = %d",DUCK_ERR_NONE);
      loginfo_ln("Running CDP Version: %s",duckutils::getCDPVersion().c_str());
      loginfo_ln("CDP source revision: %s", CDPCFG_BUILD_REVISION);
      return DUCK_ERR_NONE;
    }

    /**
     * @brief Configure LoRa radio
     * @param baudRate baud rate  matching serial monitor
     * @returns DUCK_ERR_NONE if the data was sent successfully, an error code otherwise.
     */ 
    int setupLoRaRadio(const LoRaConfigParams& config = RadioType::defaultRadioParams){
      int err = duckRadio.setupRadio(config);

      if (err == DUCKLORA_ERR_BEGIN) {
        logerr_ln("ERROR setupRadio. Starting LoRa Failed. rc = %d",err);
        return err;
      }
      if (err == DUCKLORA_ERR_SETUP) {
        logerr_ln("ERROR setupRadio. Setup LoRa Failed. rc = %d",err);
        return err;
      }
      if (err == DUCKLORA_ERR_RECEIVE) {
        logerr_ln("ERROR setupRadio. Receive LoRa Failed. rc = %d",err);
        return err;
      }
    
      loginfo_ln("setupRadio rc = %d",DUCK_ERR_NONE);
    
      return DUCK_ERR_NONE;
    }

   

    /**
     * @brief Join a visible CDP network if existing
     */ 
    void attemptNetworkJoin(){
      // Compatibility entry point: no public/private membership handshake.
      if (meshReady) router.setNetworkState(NetworkState::PUBLIC);
    }

    /**
     * @brief sendData that allows sending for reserved topic rreq
     * @returns DUCK_ERR_NONE if the data was sent successfully, an error code otherwise.
     */
    int sendRouteRequest(Duid targetDevice, RouteJSON json){
      std::string strJson = json.asString();
      std::vector<uint8_t> app_data;
      app_data.insert(app_data.end(), strJson.begin(), strJson.end());
      int err = sendReservedTopicData(targetDevice, reservedTopic::rreq, app_data);
      if (err != DUCK_ERR_NONE){
        logerr_ln("ERR: failed to send rreq");
      }
      return err;
    }

    /**
     * @brief sendData that allows sending for reserved topic rreq
     * @returns DUCK_ERR_NONE if the data was sent successfully, an error code otherwise.
     */
    int sendRouteResponse(Duid targetDevice, std::string data){
      std::vector<uint8_t> app_data;
      app_data.insert(app_data.end(), data.begin(), data.end());
      int err = sendReservedTopicData(targetDevice, reservedTopic::rrep, app_data);
      if (err != DUCK_ERR_NONE){
        logerr_ln("ERR: failed to send rrep");
      }
      return err;
    }

    /**
     * @brief sendData that allows sending for health on an internal timer
     * @returns DUCK_ERR_NONE if the data was sent successfully, an error code otherwise.
     */
    static bool sendHealth(void* p){
      Duck* duck = static_cast<Duck*>(p);
      const int err = duck->sendData(topics::health, duck->routingDiagnostics(0));
      if (err == DUCK_ERR_NONE) ++duck->counter;
      loginfo_ln("[DUCK] health enqueue rc=%d", err);
      return true;
    }

    /**
     * @brief Refresh the routing entry for a directly-heard neighbour.
     *
     * Routes are otherwise only inserted on RREQ/RREP, so a duck that is actively
     * sending data still ages out of its neighbours' tables and every later send
     * has to rediscover it with an RREQ. A packet with hopCount 0 has not been
     * relayed, so its originator is provably one hop away and reachable right now.
     */
    void refreshNeighborRoute(const CdpPacket& packet) {
      if (packet.hopCount != 0 || !packet.receivedSignal) return;
      router.insertIntoRoutingTable(packet.sduid, packet.sduid, *packet.receivedSignal, packet.timeReceived);
      if (packet.duckType == DuckType::PAPA)
        router.insertIntoRoutingTable(PAPADUCK_DUID, packet.sduid, *packet.receivedSignal, packet.timeReceived);
    }

    /**
     * @brief Periodically broadcast an RREQ so neighbours refresh our entry in
     * their routing table and their RREPs refresh theirs in ours.
     *
     * Without this a duck that has nothing to say ages out of every table after
     * CDPCFG_ROUTE_TTL_MS even though it is still reachable -- which is why the
     * TTL previously had to be set longer than the slowest telemetry beacon.
     * @returns true so the timer stays armed.
     */
    static bool sendRouteKeepalive(void* p){
      Duck* duck = static_cast<Duck*>(p);
      if (duck->getType() == DuckType::PAPA || duck->getType() == DuckType::DETECTOR) return false;
      if (duck->meshReady) {
        duck->discover(BROADCAST_DUID, 3);
        if (!duck->router.getBestNextHop(PAPADUCK_DUID) &&
            (!duck->hasRequestedRoute || uint32_t(millis() - duck->lastRreqTime) >= 30000UL))
          duck->discover(PAPADUCK_DUID, 4); // late gateway/recovery
      }
      return true;
    }

    /**
     * @brief sendData that allows sending for signal info for DMS mapping on an internal timer
     * @returns DUCK_ERR_NONE if the data was sent successfully, an error code otherwise.
     */
    static bool sendSignalData(void* p){ //move this to mama/link?
      Duck* duckInstance = static_cast<Duck*>(p);
      if (duckInstance->getType() == DuckType::PAPA) {
        logdbg_ln("[DUCK] Skipping signal info send for PapaDuck.");
        return false;
      } else if (duckInstance->getType() == DuckType::DETECTOR) {
        logdbg_ln("[DUCK] Skipping signal info send for Detector.");
        return false;
      }
      int err;
      duckInstance->router.cullRoutingTable();
      std::optional<std::string> message = duckInstance->router.getEntriesFor(PAPADUCK_DUID, duckInstance->duid);

      if(message.has_value()){
        loginfo_ln("signal data: %s", message.value().c_str());
        err = duckInstance->sendData(topics::sig, message.value());
        if (err == DUCK_ERR_NONE) duckInstance->initialSignalSent = true;
        if (err != DUCK_ERR_NONE) {
          logdbg_ln("[DUCK] signal info for DMS message failed to send.");
        } else {
          logdbg_ln("[DUCK] signal info for DMS message successfully sent.");
        }
      } else { 
        logdbg_ln("[DUCK] No route entry for specified target was found.");
        JsonDocument doc;
        std::string strSourceDuid = duckutils::hexToString(duckutils::duidAsString(duckInstance->duid));
        doc["s"] = strSourceDuid;
        JsonArray neighborsArr = doc["n"].to<JsonArray>();
        std::string jsonString;
        serializeJson(doc, jsonString);
        
        err = duckInstance->sendData(topics::sig, jsonString);
      }
      
      return true;
    }

    /**
     * @brief sendData that allows sending for reserved topic pong
     * @returns DUCK_ERR_NONE if the data was sent successfully, an error code otherwise.
     */
    int sendPong(Duid targetDevice = BROADCAST_DUID){
      std::vector<uint8_t> data(1, 0);
      int err = sendReservedTopicData(targetDevice, reservedTopic::pong, data);
      if (err != DUCK_ERR_NONE){
        logerr_ln("ERR: failed to send pong");
      }
      return err;
    }

    /**
     * @brief Enable duck radio to start receiving packets from the mesh network
     * @returns DUCK_ERR_NONE if the data was sent successfully, an error code otherwise.
     */
    int startReceive(){
      int err = duckRadio.startReceive();
      if (err != DUCK_ERR_NONE) {
        logerr_ln("ERROR Restarting Duck...");
        duckesp::restartDuck();
      }
      return err;
    };

    /**
     * @brief Log an error message if the system's memory is too low.
     */
    static void logIfLowMemory() {
      if (duckesp::getMinFreeHeap() < MEMORY_LOW_THRESHOLD
        || duckesp::getMaxAllocHeap() < MEMORY_LOW_THRESHOLD
      ) {
        //logwarn_ln("WARNING heap memory is low");
      }
    }

    /**
     * @brief Calculate a signal score based on the current RSSI and SNR values.
     * The signal score is a value between 0 and 1, where 1 is the best possible
     * signal and 0 is the worst possible signal.
     *
     * The formula used to calculate the signal score is:
     *
     * signalScore = (normalizedRSSI + normalizedSNR) / 2
     *
     * where:
     *
     * normalizedRSSI = (RSSI - RSSI_MIN) / (RSSI_MAX - RSSI_MIN)
     * normalizedSNR = (SNR - SNR_MIN) / (SNR_MAX - SNR_MIN)
     *
     * RSSI_MIN = -131 dBm
     * RSSI_MAX = -20 dBm
     * SNR_MIN = -30 dB
     * SNR_MAX = 30 dB
     *
     * @returns SignalScore struct containing rssi, snr, and signalScore values
     */
    SignalScore getSignalScore(){
      SignalScore signalInfo;
      signalInfo.rssi = (duckRadio.getRSSI() - RSSI_MIN)/(RSSI_MAX-RSSI_MIN);
      signalInfo.snr = (duckRadio.getSNR() - SNR_MIN)/(SNR_MAX-SNR_MIN);
      signalInfo.signalScore = (signalInfo.rssi + signalInfo.snr) / 2.0f;
      return signalInfo;
    }


  private:
    Duck(Duck const&) = delete;
    Duck& operator=(Duck const&) = delete;
    SizedQueue rxQueue;
    SizedQueue txQueue;
    SizedQueue reqQueue;

    bool meshReady = false, initialDiscoveryPending = false, hasRequestedRoute = false;
    bool initialSignalSent = false;
    bool preferControl = true;
    uint32_t setupAt = 0, initialDiscoveryDelay = 0, bootEpoch = 0;
    uint32_t appAccepted = 0, dataForwarded = 0, dataSuppressed = 0;
    uint32_t radioStarted = 0, radioFailed = 0, radioBusy = 0;
    uint32_t controlMalformed = 0, hopDropped = 0, controlLoopDropped = 0;
    uint32_t discoveryAttempts = 0;
    uint8_t discoveryReason = 0, diagnosticPage = 0;
    // originated/enqueued, forwarded/enqueued, duplicate suppressed, dropped.
    std::array<uint32_t, 4> requestStats{}, responseStats{};
    struct ControlSeen {
      Duid source{}; Muid id{}; uint8_t topic = 0; uint32_t at = 0; bool valid = false;
    };
    std::array<ControlSeen, 64> controlHistory{};
    size_t historyIndex = 0;

    bool controlSeen(const CdpPacket& packet) const {
      for (const auto& seen : controlHistory)
        if (seen.valid && uint32_t(millis() - seen.at) < CDPCFG_ROUTE_TTL_MS &&
            seen.source == packet.sduid && seen.id == packet.muid && seen.topic == packet.topic)
          return true;
      return false;
    }

    void rememberControl(const CdpPacket& packet) {
      controlHistory[historyIndex] = {packet.sduid, packet.muid, packet.topic, uint32_t(millis()), true};
      historyIndex = (historyIndex + 1) % controlHistory.size();
    }

    int enqueueControl(CdpPacket& packet, bool forwarded) {
      auto& stats = packet.topic == reservedTopic::rreq ? requestStats : responseStats;
      if (packet.data.empty() || packet.data.size() > MAX_DATA_LENGTH) {
        ++stats[3]; return DUCKPACKET_ERR_SIZE_INVALID;
      }
      if (packet.hopCount > MAX_HOPS) { ++stats[3]; ++hopDropped; return DUCKPACKET_ERR_MAX_HOPS; }
      if (!reqQueue.enqueue(packet)) { ++stats[3]; return DUCK_ERR_QUEUE_FULL; }
      ++stats[forwarded ? 1 : 0];
      if (!forwarded) rememberControl(packet);
      loginfo_ln("[ROUTE] enqueue t=%u src=%s mid=%s hop=%u to=%s relay=%u",
        packet.topic, duckutils::duidToString(packet.sduid).c_str(),
        duckutils::arrayToHexString(packet.muid).c_str(), packet.hopCount,
        (packet.dduid == BROADCAST_DUID ? "*" : duckutils::duidToString(packet.dduid)).c_str(), forwarded);
      return DUCK_ERR_NONE;
    }

    void discover(Duid target, uint8_t reason) {
      RouteJSON request(target, duid);
      request.addToPath(duid);
      if (sendRouteRequest(target, request) == DUCK_ERR_NONE) {
        ++discoveryAttempts; discoveryReason = reason;
        if (target != BROADCAST_DUID) {
          lastRreqTime = millis();
          hasRequestedRoute = true;
        }
      }
    }

    static bool sendRoutingDiagnostics(void* p) {
      Duck* duck = static_cast<Duck*>(p);
      if (duck->getType() == DuckType::DETECTOR) return false;
      const std::string message = duck->routingDiagnostics(duck->diagnosticPage);
      if (duck->sendData(topics::health, message) == DUCK_ERR_NONE) {
        if (duck->diagnosticPage == 0) ++duck->counter;
        duck->diagnosticPage = (duck->diagnosticPage + 1) % 4;
      }
      // One early topology snapshot makes a new installation observable without
      // waiting for the normal 195-minute signal-health timer.
      if (!duck->initialSignalSent && duck->getType() != DuckType::PAPA && duck->getType() != DuckType::DETECTOR)
        sendSignalData(duck);
      return true;
    }

    void observeVia(const CdpPacket& packet, Duid via) {
      if (!packet.receivedSignal || via == duid || via == PAPADUCK_DUID || via == BROADCAST_DUID) return;
      router.insertIntoRoutingTable(via, via, *packet.receivedSignal, packet.timeReceived);
    }

    bool handleRoutingPacket(CdpPacket packet) {
      if (getType() == DuckType::DETECTOR) return false;
      if (packet.topic != reservedTopic::rreq && packet.topic != reservedTopic::rrep) return false;
      auto& stats = packet.topic == reservedTopic::rreq ? requestStats : responseStats;
      if (packet.hopCount > MAX_HOPS) { ++stats[3]; ++hopDropped; return true; }
      auto parsed = RouteJSON::fromPacketData(packet.data);
      if (!parsed) { ++stats[3]; ++controlMalformed; return true; }
      RouteJSON& route = *parsed;
      if (packet.topic == reservedTopic::rreq) {
        const auto via = route.getlastInPath();
        if (!via || !route.startsAt(packet.sduid) || route.getOrigin() != packet.sduid || route.getDestination() != packet.dduid ||
            route.pathSize() != size_t(packet.hopCount) + 1) {
          ++stats[3]; ++controlMalformed; return true;
        }
        observeVia(packet, *via);
        if (controlSeen(packet)) { ++stats[2]; return true; }
        rememberControl(packet);
        if (route.contains(duid)) { ++stats[3]; ++controlLoopDropped; return true; }
        if (packet.receivedSignal)
          router.insertIntoRoutingTable(route.getOrigin(), *via, *packet.receivedSignal, packet.timeReceived);
        const bool broadcast = packet.dduid == BROADCAST_DUID;
        const bool destination = packet.dduid == duid ||
            (packet.dduid == PAPADUCK_DUID && getType() == DuckType::PAPA);
        if ((broadcast && packet.hopCount == 0) || destination) {
          auto reply = route.responseFrom(getType() == DuckType::PAPA ? PAPADUCK_DUID : duid, packet.muid);
          reply.setVia(duid);
          sendRouteResponse(*via, reply.asString());
        } else if (!broadcast) {
          if (getType() == DuckType::LINK) return true; // leaf, never advertise a transit path
          if (packet.hopCount >= MAX_HOPS) { ++stats[3]; ++hopDropped; return true; }
          route.addToPath(duid);
          packet.data = duckutils::stringToByteVector(route.asString());
          ++packet.hopCount;
          enqueueControl(packet, true);
        }
      } else {
        auto via = route.getVia();
        if (!via && packet.hopCount == 0) via = packet.sduid;
        if (via) observeVia(packet, *via);
        // Overhearing a unicast reply does not authorize regenerating it.
        if (packet.dduid != duid) return true;
        if (controlSeen(packet)) { ++stats[2]; return true; }
        rememberControl(packet);
        const bool legacyDirect = !route.getVia() && packet.hopCount == 0 && route.getDestination() == duid;
        if (!via || (!legacyDirect && (!route.startsAt(route.getDestination()) ||
            route.getlastInPath() != std::optional<Duid>(duid)))) {
          ++stats[3]; ++controlMalformed; return true;
        }
        if (packet.receivedSignal) {
          router.insertIntoRoutingTable(route.getOrigin(), *via, *packet.receivedSignal, packet.timeReceived);
          if (packet.duckType == DuckType::PAPA) {
            router.insertIntoRoutingTable(PAPADUCK_DUID, *via, *packet.receivedSignal, packet.timeReceived);
            router.insertIntoRoutingTable(packet.sduid, *via, *packet.receivedSignal, packet.timeReceived);
          }
        }
        if (route.getDestination() == duid) return true;
        if (getType() == DuckType::LINK) return true;
        route.popFromPath();
        const auto previous = route.getlastInPath();
        if (packet.hopCount >= MAX_HOPS) { ++stats[3]; ++hopDropped; return true; }
        if (!previous || *previous == *via) {
          ++stats[3]; ++controlLoopDropped; return true;
        }
        packet.dduid = *previous;
        route.setVia(duid);
        packet.data = duckutils::stringToByteVector(route.asString());
        ++packet.hopCount;
        // Preserve responder, MUID and accumulated hops through the return path.
        enqueueControl(packet, true);
      }
      return true;
    }

    void processReceivedPacket(CdpPacket& packet) {
      if (handleRoutingPacket(packet)) return;
      if (packet.hopCount > MAX_HOPS) { ++hopDropped; return; }
      refreshNeighborRoute(packet);
      handleReceivedPacket(packet);
    }

    //Telemetry
    const int HEALTH_INTERVAL = (1000 * 60 * 60 * 2) + (1000 * 60 * 12) ; //2 Hours 12 Minutes
    const int SIGNAL_INTERVAL = (1000 * 60 * 60 * 3) + (1000 * 60 * 15); //3 hours 15 minutes
    uint32_t counter = 1;
    Timer<10> duckTimer;

    /** 
     * @brief Read packets from CDP nodes responding to our network join request
     * @returns Optional<CdpPacket> if network join response is found, nullopt if not 
     */
    std::optional<CdpPacket> checkForNetworks(){ return std::nullopt; }

    /**
     * @brief Control access for public APIs to send certain reserved topic types
     *
     * @param targetDevice device uid to send to
     * @param topic reserved topic of data
     * @param data byte vector representing data to send
     * @return DUCK_ERR_NONE if the data was sent successfully, an error code otherwise.
     */
    int sendReservedTopicData(Duid targetDevice, reservedTopic topic, std::vector<uint8_t> data){
      if (!meshReady) return DUCK_ERR_NOT_SUPPORTED;
      if (data.empty() || data.size() > MAX_DATA_LENGTH) return DUCKPACKET_ERR_SIZE_INVALID;
      CdpPacket packet(targetDevice, topic, data, duid, getType());
      router.getFilter().assignUniqueMessageId(packet);
      if (topic == reservedTopic::rreq || topic == reservedTopic::rrep)
        return enqueueControl(packet, false);
      return txQueue.enqueue(packet) ? DUCK_ERR_NONE : DUCK_ERR_QUEUE_FULL;
    }

    /**
     * @brief Passes data to radiolib for rx
     *
     * @param txPacket CdpPacket to be sent 
     * @return DUCK_ERR_NONE if the data was sent successfully, an error code otherwise.
     */
    int sendToRadio(CdpPacket& txPacket) {
      int err = txPacket.prepareForSending();
      if (err != DUCK_ERR_NONE) {
        logerr_ln("ERROR Failed to build ping packet: %s, err = %i",getDuckErrorString(err), err);
        return err;
      }

      router.getFilter().bloom_add(txPacket.muid.data(), MUID_LENGTH);

      err = duckRadio.sendData(txPacket.asBytes());
      if (err != DUCK_ERR_NONE) {
        logerr_ln("ERROR Lora sendData failed, err = %d", err);
      }
     
      return err;
    }

    void queueReceivedPacket(){
        std::optional<std::vector<uint8_t>> rxData = this->duckRadio.readReceivedData();
        if (!rxData.has_value()) {
          logerr_ln("ERROR failed to get data from DuckRadio.");
          return;
        }
        CdpPacket rxPacket(rxData.value());
        logdbg_ln("Got data from radio. size: %d",rxPacket.size());
        rxPacket.timeReceived = millis();
        rxPacket.receivedSignal = getSignalScore();
        rxQueue.enqueue(rxPacket);
        //move handle receieve packet to duck base, turn old handlereceive into route protocol?
    }
    
};

#endif
