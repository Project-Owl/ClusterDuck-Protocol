/**
 * @file RouteJSON.h
 * @brief This file is internal to CDP and provides route JSON construction
 *  and manipulation
 * @version
 * @date 2025-7-24
 *
 * @copyright
 */
#ifndef RouteJSON_H
#define RouteJSON_H

#include <ArduinoJson.h>
#include <optional>
#include <algorithm>
#include "../utils/DuckUtils.h"
#include "../CdpPacket.h"

class RouteJSON {
    public:
        /**
         * @brief Construct a new Route JSON object
         *
         * @param targetDevice the destination device DUID
         * @param sourceDevice the source device DUID
         */
        RouteJSON(Duid targetDevice, Duid sourceDevice) {
            origin = duckutils::duidToString(sourceDevice);
            // Raw 0xFF bytes are not a valid UTF-8 JSON string.
            destination = targetDevice == BROADCAST_DUID ? "*" : duckutils::duidToString(targetDevice);
            json["origin"] = origin;
            json["destination"] = destination;
            json["path"].to<ArduinoJson::JsonArray>();

            std::string log;
            serializeJson(json, log);
            loginfo_ln("RouteDoc: %s", log.c_str());
        }

        /**
         * @brief Parse route JSON out of received packet data.
         *
         * This is the only way to build a RouteJSON from over-the-air data. It
         * returns nullopt and logs the specific reason when the payload is
         * malformed, so a caller cannot end up holding a half-parsed document --
         * there is no validity flag left to forget to check.
         *
         * @param packetData the received packet data as a byte vector
         * @returns the parsed document, or nullopt if the payload is not usable
         */
        static std::optional<RouteJSON> fromPacketData(const std::vector<uint8_t>& packetData) {
            RouteJSON doc;
            if (!doc.parsePacketData(packetData)) {
                return std::nullopt;
            }
            return doc;
        }

        std::string asString(){
            std::string out;
            serializeJson(json, out);
            return out;
        }

        std::string convertReqToRep(){
            std::string oldOrigin = origin;
            //update rreq to rrep
            origin = destination;
            destination = oldOrigin;
            json["origin"] = origin;
            json["destination"] = destination;

            std::string log;
            serializeJson(json, log);

            return asString();
        }
        Duid getOrigin(){
            Duid originDuid;
            originDuid.fill(0);
            if (origin.size() == DUID_LENGTH) {
                std::copy(origin.begin(), origin.end(), originDuid.begin());
            }
            return originDuid;
        }
        Duid getDestination(){
            if (destination == "*") return BROADCAST_DUID;
            Duid destinationDuid;
            destinationDuid.fill(0);
            if (destination.size() == DUID_LENGTH) {
                std::copy(destination.begin(), destination.end(), destinationDuid.begin());
            }
            return destinationDuid;
        }

        /**
         * @brief add a duck node to the path to route the request path
         *
         * @param deviceId of the duck node being added
         * @return the newly modified Arduino JSON document
         */
        std::string addToPath(Duid deviceId){
            objPath.push_back(duckutils::duidToString(deviceId));
            json["path"].to<ArduinoJson::JsonArray>(); //.to erases content of the field in the doc, but .as does not modify the doc at all.
            updateJsonPath(); //so we will manually copy the local obj path to the doc
#ifdef CDP_LOG_DEBUG
            std::string log;
            serializeJson(json, log);
            logdbg_ln("RREQ: %s", log.c_str());
#endif
            return asString();
            //add rssi snr
        }

        std::optional<Duid> getlastInPath(){
            Duid lastDuid;
            if(objPath.size() > 0){
                const std::string& last = objPath[objPath.size()-1];
                if (last.size() != DUID_LENGTH) {
                    logdbg_ln("RREQ path entry has invalid length, ignoring");
                    return std::nullopt;
                }
                std::copy(last.begin(), last.end(), lastDuid.begin());
                return lastDuid;

            } else{
                logdbg_ln("RREQ path empty, filling with self");
                return std::nullopt;
            }
        }

    /**
     * @brief pop the last duck node from the route response path
     *
     * @return the newly modified Arduino JSON document
     */
    std::string popFromPath(){
        if (objPath.empty()) {
            //pop_back on an empty vector is UB
            logdbg_ln("popFromPath: path already empty, nothing to pop");
            return asString();
        }
        objPath.pop_back();
        updateJsonPath();

        std::string log;
        serializeJson(json, log);
        logdbg_ln("Packet: %s", log.c_str());

        return asString();
    }

        size_t pathSize() const { return objPath.size(); }
        bool contains(Duid id) const {
            return std::find(objPath.begin(), objPath.end(), duckutils::duidToString(id)) != objPath.end();
        }
        bool startsAt(Duid id) const {
            return !objPath.empty() && objPath.front() == duckutils::duidToString(id);
        }
        std::optional<Duid> getVia() const {
            const std::string via = json["via"].as<std::string>();
            if (via.size() != DUID_LENGTH) return std::nullopt;
            Duid id{};
            std::copy(via.begin(), via.end(), id.begin());
            return id;
        }
        void setVia(Duid id) { json["via"] = duckutils::duidToString(id); }
        RouteJSON responseFrom(Duid responder, Muid requestId) const {
            RouteJSON response(*this);
            response.convertReqToRep();
            response.origin = duckutils::duidToString(responder);
            response.json["origin"] = response.origin;
            response.json["q"] = duckutils::arrayToHexString(requestId);
            return response;
        }

  private:
        /// Only fromPacketData() may build an unpopulated document.
        RouteJSON() = default;

        /**
         * @brief Populate this document from received packet data.
         *
         * Every field that is later copied into a fixed-size Duid is length
         * checked here, so getOrigin()/getDestination()/getlastInPath() cannot
         * over-run their destination buffers with attacker-shaped JSON.
         *
         * @returns false if the payload is malformed; the reason is logged.
         */
        bool parsePacketData(const std::vector<uint8_t>& packetData) {
            if (packetData.empty() || packetData.size() > MAX_DATA_LENGTH) return false;
            std::string packetStr(packetData.begin(), packetData.end());
            DeserializationError error = deserializeJson(json, packetStr);
            if (error) {
                logerr_ln("RouteJSON deserialization failed: %s", error.c_str());
                return false;
            }

            const char* originPtr = json["origin"].as<const char*>();
            const char* destinationPtr = json["destination"].as<const char*>();
            if (originPtr == nullptr || destinationPtr == nullptr) {
                logerr_ln("RouteJSON missing origin/destination");
                return false;
            }
            origin = json["origin"].as<std::string>();
            destination = json["destination"].as<std::string>();
            if (origin.size() != DUID_LENGTH ||
                (destination.size() != DUID_LENGTH && destination != "*")) {
                logerr_ln("RouteJSON origin/destination length invalid (%d/%d)",
                          (int)origin.size(), (int)destination.size());
                return false;
            }

            if (!printableDuid(origin) ||
                (destination != "*" && destination != duckutils::duidToString(BROADCAST_DUID) && !printableDuid(destination))) return false;
            if (!json["path"].is<JsonArray>() || json["path"].size() > MAX_HOPS + 1) return false;
            if (json["via"].isNull() == false && !printableDuid(json["via"].as<std::string>())) return false;
            if (json["q"].isNull() == false && json["q"].as<std::string>().size() != MUID_LENGTH * 2) return false;
            for (JsonVariant value : json["path"].as<JsonArray>()) {
                std::string entry = value.as<std::string>();
                if (!printableDuid(entry) ||
                    std::find(objPath.begin(), objPath.end(), entry) != objPath.end()) {
                    logerr_ln("RouteJSON path entry length invalid (%d)", (int)entry.size());
                    return false;
                }
                objPath.push_back(entry);
            }

            const std::string serialized = asString();
            logdbg_ln("Built RouteJSON from packet data: %s", serialized.c_str());
            return true;
        }

        ArduinoJson::JsonDocument json;
        static bool printableDuid(const std::string& value) {
            return value.size() == DUID_LENGTH && std::all_of(value.begin(), value.end(),
                [](unsigned char c) { return c >= 0x20 && c <= 0x7e; });
        }
        std::vector<std::string> objPath;
        std::string origin;
        std::string destination;

        void updateJsonPath(){
            JsonArray path = json["path"].to<JsonArray>();
            path.clear();

            for (const auto& s : objPath) {
                path.add(s);
            }
        }
  };

  #endif
