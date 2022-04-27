#ifndef CIRCULAR_BUFFER_STRING_H
#define CIRCULAR_BUFFER_STRING_H

#include <CdpPacket.h>

class CircularBufferString{
private:
    int head;
    int tail;
    int count;
    int buffer_end;
    std::string* buffer;
public:
    CircularBufferString(int size);

    ~CircularBufferString();

   /**
   * @brief push message into buffer
   *
   * @param msg the packet to add to the buffer
   */
    void push(std::string msg);

   /**
   * @brief retrieve head of the circular buffer
   * 
   * @returns index of the head element
   */
    int getHead();

   /**
   * @brief retrieve tail of the circular buffer
   * 
   * @returns index of the tail element
   */
    int getTail();

   /**
   * @brief retrieve the end of the circular buffer
   * 
   * @returns index of the 
   */
    int getBufferEnd();

   /**
   * @brief retrieve a specific message from the buffer
   *
   * @param index the index of the message to retrieve
   * 
   * @returns the message packet at the provided index
   */
    std::string getMessage(int index);

};

#endif