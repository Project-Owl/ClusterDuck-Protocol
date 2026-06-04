/**
 * @file DuckLoRa.h
 * @brief This file is internal to CDP and provides direct routing
 *functionality and a routing table
 * @version
 * @date 2025-7-24
 *
 * @copyright
 */
#ifndef DUCKROUTER_H_
#define DUCKROUTER_H_

#include <map>
#include <unordered_map>
#include <list>
#include <optional>
#include "../include/cdpcfg.h"
#include "bloomfilter.h"
#include "Neighbor.h"
enum class NetworkState {SEARCHING, PUBLIC, DISCONNECTED};

class DuckRouter {
    public:
        DuckRouter() = default;
        ~DuckRouter() = default;
        BloomFilter& getFilter();
        NetworkState getNetworkState(){ return networkState; };

                /**
         * @brief Insert a new record into the routing table. Does not sort Neighbor list, but updates last seen timestamp if device already exists in the list.
         * Routing score is calculated based on signal info and may be updated in existing record if new score is better than old score.
         *
         * @param deviceID the device ID
         * @param lastSeen the last seen timestamp
         * @param signalInfo the signal information (SNR & RSSI) used to sort the @p nextHop elements in the routing table
         */
        void insertIntoRoutingTable(Duid deviceID, Duid nextHop, SignalScore signalInfo,
                                    uint32_t heardAt = millis());
    /**
* @brief Get the best next hop DUID for reaching a target device.
*
* The routing table stores, for each destination device, a list of Neighbor
* records (likely representing possible next hops and their signal scores /
* last-seen timestamps). This function:
* - Looks up the routing table entry for @p targetDeviceId.
* - If an entry exists, sorts the neighbor list by Neighbor's operator> (so
*   presumably highest-quality neighbors come first).
* - Returns the device id of the top neighbor as a Duid.
*
* Notes and caveats:
* - If no routing entry exists for the requested target, returns std::nullopt.
* - Sorting is done in-place (side effect) on the stored neighbor list.
*
* @param targetDeviceId The destination device identifier for which a next hop
*                       is requested.
* @return std::optional<Duid> The Duid of the selected next hop, or
*                             std::nullopt if none exists.
*/
        std::optional<Duid> getBestNextHop(Duid targetDeviceId);
        std::optional<uint32_t> getRouteAge(Duid targetDeviceId);
        size_t directNeighborCount();
        uint32_t transitions = 0;
        uint32_t neighborEvictions = 0;
        // observed, included and millis() of the last signal serialization.
        std::optional<std::array<uint32_t, 3>> signalSnapshot;

        /**
         * @brief NetworkState if the Duck joins or disconnects from a CDP network
         * @param newState The new NetworkState to join
         */
        void setNetworkState(NetworkState newState){
            if (networkState != newState) {
                NetworkState oldState = networkState;
                networkTransition(oldState, newState);
                if (networkState != oldState) ++transitions;
            }
        }

                /**
         * @brief Cull the routing table to a maximum size and remove expired routes. Default is 3 entries.
         * @param maxSize the maximum size of neighbor entries per destination target
         */
        void cullRoutingTable(size_t maxSize = 3);

        /**
         * @brief retrieve all neighbor entries for a given destination target
         * @param targetDuid destination duid
         * @returns the list of all best-next-hops for the destination id in string format, to be sent as packet data.
         */
        std::optional<std::string> getEntriesFor(Duid targetDuid, Duid thisDuck);

    private:
        std::unordered_map<std::string, std::list<Neighbor>> routingTable;
        // A destination changing next hop must never overwrite a measured link.
        std::unordered_map<std::string, Neighbor> directNeighbors;
        BloomFilter filter;
        NetworkState networkState = NetworkState::SEARCHING;
        unsigned long ROUTE_TTL = CDPCFG_ROUTE_TTL_MS;

        /**
         * @brief NetworkState transition for NetworkState FSM
         * @param oldState NetworkState to transition out of
         * @param newState NetworkState to transition in to
         */
        void networkTransition(NetworkState oldState, NetworkState newState){
            if (oldState == NetworkState::SEARCHING && newState == NetworkState::PUBLIC) {
                loginfo_ln("[ROUTER] Public network joined.");
                networkState = newState;
            } else if (oldState == NetworkState::PUBLIC && newState == NetworkState::DISCONNECTED){
                networkState = newState;
                loginfo_ln("[ROUTER] Successfully disconnected from CDP network.");
            } else if(oldState == NetworkState::PUBLIC && newState == NetworkState::SEARCHING){
                networkState = newState;
                loginfo_ln("[ROUTER] Lost connection to CDP Network.");
            } else if(oldState == NetworkState::DISCONNECTED && newState == NetworkState::SEARCHING){
                networkState = newState;
                logdbg_ln("[ROUTER] Leaving disconnected state, looking for CDP networks.");
            } else {
                logdbg_ln("[ROUTER] Invalid network state transition!");
            }
        }
        
};
  #endif
