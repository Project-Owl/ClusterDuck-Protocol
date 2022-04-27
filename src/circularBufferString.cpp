#include "include/circularBufferString.h"
#include "DuckLogger.h"

//when initializing the buffer, the buffer created will be one larger than the provided size
//one slot in the buffer is used as a waste slot
CircularBufferString::CircularBufferString(int size)
{
    buffer = new std::string[size + 1];
    head = 0;
    buffer_end = size + 1;
    tail = 0;
    count = 0;
}
void CircularBufferString::push(std::string msg)
{
    buffer[head] = msg;
    head = head+ 1;
    if(head == buffer_end) head = 0;
    if(head == tail) tail+= 1;
    if(tail == buffer_end) tail = 0;
}
int CircularBufferString::getHead(){
    return head;
}
int CircularBufferString::getTail(){
    return tail;
}
int CircularBufferString::getBufferEnd(){
    return buffer_end;
}

std::string CircularBufferString::getMessage(int index)
{
    return buffer[index];
}
CircularBufferString::~CircularBufferString()
{
    delete buffer;
}


