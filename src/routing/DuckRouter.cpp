#include "DuckRouter.h"
#include <ArduinoJson.h>
#include <algorithm>
#include <vector>

void DuckRouter::insertIntoRoutingTable(Duid deviceID, Duid nextHop, SignalScore signalInfo) {

    Neighbor neighborRecord(deviceID, nextHop, signalInfo, millis());
    auto index = routingTable.find(duckutils::hexToString(duckutils::duidAsString(deviceID)));
    if (index == routingTable.end()) { //need to make sure we aren't adding Link1276->Link1276 to Mama1262
        std::list<Neighbor> neighborList;
        neighborList.push_back(neighborRecord);
        routingTable.insert(std::make_pair(neighborRecord.getDeviceId(), neighborList));
    } else {
        // Update existing record
        index->second.remove_if([neighborRecord](const Neighbor& n) {
            return n.getLastSeen() < neighborRecord.getLastSeen() && n.getDeviceId() == neighborRecord.getDeviceId();
        });
        index->second.push_back(neighborRecord);
    }
};

std::optional<Duid> DuckRouter::getBestNextHop(Duid targetDeviceId){
    //check if nextHop = the duid of the last duid in path/last duid that relayed to the current duck so that it doesn't transmit back the way it came from
    auto nextHopRecord = routingTable.find(duckutils::hexToString(duckutils::duidAsString(targetDeviceId)));
    if (nextHopRecord == routingTable.end()) {
        return std::nullopt; // No entry found
    }
    nextHopRecord->second.sort(std::greater<>()); 
    std::string nextHopStr = nextHopRecord->second.front().getDeviceId();
    Duid nextHopId;
    std::copy(nextHopStr.begin(), nextHopStr.end(),nextHopId.begin());

    if ((uint32_t)(millis() - nextHopRecord->second.front().getLastSeen()) >= ROUTE_TTL) {
        Serial.printf(
            "millis=%lu lastSeen=%lu ttl=%lu threshold=%lu\n",
            millis(),
            nextHopRecord->second.front().getLastSeen(),
            ROUTE_TTL,
            millis() - ROUTE_TTL
        );
        Serial.println(nextHopStr.c_str());
              loginfo_ln("[ROUTER] route ttl expired");
              routingTable.erase(nextHopStr);
              return std::nullopt;
    }
    return nextHopId;

};

void DuckRouter::cullRoutingTable(size_t maxSize) {
    //iterate through and remove all expired ttl routes based off of the route ttl expiry above

    auto neighborIndex = routingTable.begin();
    while(neighborIndex != routingTable.end()) {
        auto& neighborList = neighborIndex->second;
        
        auto entry = neighborList.begin();
        while(entry != neighborList.end()) {
            if ((uint32_t)(millis() - entry->getLastSeen()) >= ROUTE_TTL) {
                loginfo_ln("[ROUTER] culling route with ttl expired");
                entry = neighborList.erase(entry);
            } else {
                ++entry;
            }
        }
        //check to make sure entries per destination doesn't exceed max
        neighborList.sort(std::greater<>()); // remove this when you add sorting to routing table insert?
        while (neighborList.size() > maxSize) {
            loginfo_ln("[ROUTER] culling route that exceeded max neighbors");
            neighborList.pop_back();
        }

        //empty check needs to go last so that neighborList isn't deleted
        if (neighborList.empty()) {
            neighborIndex = routingTable.erase(neighborIndex);
        } else {
            ++neighborIndex;
        }
    }
};

std::optional<std::string> DuckRouter::getEntriesFor(Duid targetDuid, Duid thisDuck){
    // targetDuid is retained for API compatibility but no longer filters. It used
    // to look up only the papa key, so the signal-health payload reported papa as
    // the single "neighbour" on every node and the real per-neighbour rssi/snr --
    // which is already in the table under each sender's own duid -- was never read.
    (void)targetDuid;

    std::vector<Neighbor> direct;
    for (const auto& record : routingTable) {
        for (const auto& entry : record.second) {
            if (entry.isDirectNeighbor()) {
                direct.push_back(entry);
            }
        }
    }

    if (direct.empty()) {
        return std::nullopt;
    }

    // Best signal first, so if the payload has to be truncated we keep the links
    // that matter.
    std::sort(direct.begin(), direct.end(),
              [](const Neighbor& a, const Neighbor& b) { return a > b; });

    const size_t reported = std::min(direct.size(), (size_t)CDPCFG_SIGNAL_MAX_NEIGHBORS);
    if (direct.size() > reported) {
        loginfo_ln("[ROUTER] signal health reporting %u of %u neighbors (payload cap)",
                   (unsigned)reported, (unsigned)direct.size());
    }

    JsonDocument doc;
    doc["s"] = duckutils::hexToString(duckutils::duidAsString(thisDuck));
    JsonArray neighborsArr = doc["n"].to<JsonArray>();

    for (size_t i = 0; i < reported; i++) {
        JsonArray node = neighborsArr.createNestedArray();
        node.add(duckutils::hexToString(duckutils::duidAsString(direct[i].getDuid())));
        node.add(direct[i].getRssi());
        node.add(direct[i].getSnr());
    }

    std::string jsonString;
    serializeJson(doc, jsonString);

    if (jsonString.size() > MAX_DATA_LENGTH) {
        logerr_ln("[ROUTER] signal health payload %u bytes exceeds %u; lower CDPCFG_SIGNAL_MAX_NEIGHBORS",
                  (unsigned)jsonString.size(), (unsigned)MAX_DATA_LENGTH);
    }

    return jsonString;
}

BloomFilter& DuckRouter::getFilter(){
    return filter; //just call the bloomfilter function here?
};
