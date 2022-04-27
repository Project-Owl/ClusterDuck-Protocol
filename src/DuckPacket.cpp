#include "Arduino.h"
#include "include/DuckPacket.h"
#include "DuckError.h"
#include "DuckLogger.h"
#include "MemoryFree.h"
#include "include/DuckUtils.h"
#include "include/DuckCrypto.h"
#include "include/bloomfilter.h"
#include <string>

// Function that slices vectors
// vector slicing(vector<byte>& arr,
//                     int X, int Y)
// {
 
//     // Starting and Ending iterators
//     auto start = arr.begin() + X;
//     auto end = arr.begin() + Y + 1;
 
//     // To store the sliced vector
//     vector<int> result(Y - X + 1);
 
//     // Copy vector using copy function()
//     copy(start, end, result.begin());
 
//     // Return the final sliced vector
//     return result;
// }

// vector slice(vector<byte>)

bool DuckPacket::prepareForRelaying(BloomFilter *filter, std::vector<byte> dataBuffer) {


  this->reset();

  loginfo("prepareForRelaying: START");
  loginfo("prepareForRelaying: Packet is built. Checking for relay...");

  // Query the existence of strings
  bool alreadySeen = filter->bloom_check(&dataBuffer[MUID_POS], MUID_LENGTH);
  if (alreadySeen) {
    logdbg("handleReceivedPacket: Packet already seen. No relay.");
    return false;
  } else {
    filter->bloom_add(&dataBuffer[MUID_POS], MUID_LENGTH);
    logdbg("handleReceivedPacket: Relaying packet: "  + duckutils::convertToHex(&dataBuffer[MUID_POS], MUID_LENGTH));
  }

  // update the rx packet internal byte buffer
  buffer.assign(dataBuffer.begin(), dataBuffer.end());
  int hops = buffer[HOP_COUNT_POS]++;
  loginfo("prepareForRelaying: hops count: "+ String(hops));
  return true;
  
  
}

bool DuckPacket::prepareForRelaying(BloomFilter *filter, std::vector<byte> dataBuffer, int rssi) {


  this->reset();

  loginfo("prepareForRelaying: START");
  loginfo("prepareForRelaying: Packet is built. Checking for relay...");

  //TODO: Add bloom filter empty when full
  //TODO: Add 2nd bloom filter
  //TODO: Calculate false positive chance
  //TODO: Add backwards compatibility

  // Query the existence of strings
  bool alreadySeen = filter->bloom_check(&dataBuffer[MUID_POS], MUID_LENGTH);
  if (alreadySeen) {
    logdbg("handleReceivedPacket: Packet already seen. No relay.");
    return false;
  } else {
    filter->bloom_add(&dataBuffer[MUID_POS], MUID_LENGTH);
    logdbg("handleReceivedPacket: Relaying packet: "  + duckutils::convertToHex(&dataBuffer[MUID_POS], MUID_LENGTH));
  }

  // update the rx packet internal byte buffer
  buffer.assign(dataBuffer.begin(), dataBuffer.end());
  int hops = buffer[HOP_COUNT_POS]++;
  logdbg("prepareForRelaying: hops count: "+ String(hops));

  if(hops <= 1) {
    loginfo("hops count: "+ String(hops));
    // std::vector<byte> temp;
    // temp = slicing(dataBuffer, DATA_POS, dataBuffer.end())
    std::vector<byte> newDataBuff;
    newDataBuff.insert(newDataBuff.end(), dataBuffer.begin(), dataBuffer.end());
    std::vector<byte> rssiAdd;
    byte rssiVal = rssi;

    // Adds DUID
    rssiAdd.insert(rssiAdd.begin(), duid.begin(), duid.end());
    
    // Adds RSSI
    rssiAdd.push_back(rssi);

    byte crc_bytes[DATA_CRC_LENGTH];
    uint32_t value;

    value = CRC32::calculate(newDataBuff.data(), newDataBuff.size());

    crc_bytes[0] = (value >> 24) & 0xFF;
    crc_bytes[1] = (value >> 16) & 0xFF;
    crc_bytes[2] = (value >> 8) & 0xFF;
    crc_bytes[3] = value & 0xFF;

    for(int i = 0; i < 4; i++) {
      dataBuffer[DATA_CRC_POS+i] = crc_bytes[i];
    }

    logdbg(rssi);
    dataBuffer.insert(dataBuffer.end(), rssiAdd.begin(), rssiAdd.end());
    // logdbg("RSSI:      " + duckutils::convertToHex(dataBuffer.data(), dataBuffer.size()));
  }

  return true;
  
  
}

void DuckPacket::getUniqueMessageId(BloomFilter * filter, byte message_id[MUID_LENGTH]) {

  bool getNewUnique = true;
  while (getNewUnique) {
    duckutils::getRandomBytes(MUID_LENGTH, message_id);
    getNewUnique = filter->bloom_check(message_id, MUID_LENGTH);
    loginfo("prepareForSending: new MUID -> " + duckutils::convertToHex(message_id, MUID_LENGTH));
    
  }
}

int DuckPacket::prepareForSending(BloomFilter *filter,
                                  std::vector<byte> targetDevice, byte duckType,
                                  byte topic, std::vector<byte> app_data) {

  std::vector<uint8_t> encryptedData;
  uint8_t app_data_length = app_data.size();

  this->reset();

  if (app_data_length > MAX_DATA_LENGTH) {
    return DUCKPACKET_ERR_SIZE_INVALID;
  }

  loginfo("prepareForSending: DATA LENGTH: " + String(app_data_length) +
          " TOPIC: " + String(topic));

  byte message_id[MUID_LENGTH];
  getUniqueMessageId(filter, message_id);

  byte crc_bytes[DATA_CRC_LENGTH];
  uint32_t value;
  // TODO: update the CRC32 library to return crc as a byte array
  if(duckcrypto::getState()) {
    encryptedData.resize(app_data.size());
    duckcrypto::encryptData(app_data.data(), encryptedData.data(), app_data.size());
    value = CRC32::calculate(encryptedData.data(), encryptedData.size());
  } else {
    value = CRC32::calculate(app_data.data(), app_data.size());
  }
  
  crc_bytes[0] = (value >> 24) & 0xFF;
  crc_bytes[1] = (value >> 16) & 0xFF;
  crc_bytes[2] = (value >> 8) & 0xFF;
  crc_bytes[3] = value & 0xFF;

  // ----- insert packet header  -----
  // source device uid
  buffer.insert(buffer.end(), duid.begin(), duid.end());
  logdbg("SDuid:     " + duckutils::convertToHex(duid.data(), duid.size()));

  // destination device uid
  buffer.insert(buffer.end(), targetDevice.begin(), targetDevice.end());
  logdbg("DDuid:     " + duckutils::convertToHex(targetDevice.data(), targetDevice.size()));

  // message uid
  buffer.insert(buffer.end(), &message_id[0], &message_id[MUID_LENGTH]);
  logdbg("Muid:      " + duckutils::convertToHex(buffer.data(), buffer.size()));

  // topic
  buffer.insert(buffer.end(), topic);
  logdbg("Topic:     " + duckutils::convertToHex(buffer.data(), buffer.size()));

  // duckType
  buffer.insert(buffer.end(), duckType);
  logdbg("duck type: " + duckutils::convertToHex(buffer.data(), buffer.size()));

  // hop count
  buffer.insert(buffer.end(), 0x00);
  logdbg("hop count: " + duckutils::convertToHex(buffer.data(), buffer.size()));

  // data crc
  buffer.insert(buffer.end(), &crc_bytes[0], &crc_bytes[DATA_CRC_LENGTH]);
  logdbg("Data CRC:  " + duckutils::convertToHex(buffer.data(), buffer.size()));

  // ----- insert data -----
  if(duckcrypto::getState()) {

    buffer.insert(buffer.end(), encryptedData.begin(), encryptedData.end());
    logdbg("Encrypted Data:      " + duckutils::convertToHex(buffer.data(), buffer.size()));

  } else {
    buffer.insert(buffer.end(), app_data.begin(), app_data.end());
    logdbg("Data:      " + duckutils::convertToHex(buffer.data(), buffer.size()));
  }
  
  // ----- insert path -----
  // buffer.insert(buffer.end(), duid.begin(), duid.end());
  // logdbg("Path:      " + duckutils::convertToHex(buffer.data(), buffer.size()));

  logdbg("Built packet: " +
         duckutils::convertToHex(buffer.data(), buffer.size()));
  return DUCK_ERR_NONE;
}

