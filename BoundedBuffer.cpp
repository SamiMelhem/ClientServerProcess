// your PA3 BoundedBuffer.cpp code here
#include "BoundedBuffer.h"
#include <mutex>
#include <condition_variable>
#include <cstring>
#include <cassert>

using namespace std;

BoundedBuffer::BoundedBuffer (int _cap) : cap(_cap) {
    // modify as needed
}

BoundedBuffer::~BoundedBuffer () {
    // modify as needed
}

void BoundedBuffer::push (char* msg, int size) {
    // if (msg == nullptr || size <= 0) { // Handle error: invalid input
    //     return;
    // }
    // 1. Convert the incoming byte sequence given by msg and size into a vector<char>
    vector<char> message(msg, msg + size);
    unique_lock<mutex> lock(mtx);

    // 2. Wait until there is room in the queue (i.e., queue length is less than cap)
    slot_available.wait(lock, [this]{ return q.size() < (size_t) cap; });
    // while (q.size() >= static_cast<size_t>(cap))
    // slot_available.wait(lock); // Wait until notified

    // 3. Then push the vector at the end of the queue
    q.push(message);
    // lock.unlock();

    // 4. Wake up threads that were waiting for push
    data_available.notify_one(); // Wakes up consumer threads one at a time?
}

int BoundedBuffer::pop (char* msg, int size) {
    unique_lock<mutex> lock(mtx);

    // 1. Wait until the queue has at least 1 item
    data_available.wait(lock, [this]{ return q.size() > 0; }); // not q.empty()
    // while (q.empty()) { data_available.wait(lock); }

    // 2. Pop the front item of the queue. The popped item is a vector<char>
    vector<char> data = q.front();
    q.pop();
    // lock.unlock();

    // 3. Convert the popped vector<char> into a char*, copy that into msg; assert that the vector<char>'s length is <= size
    char* poppedVector = data.data();
    size_t length = data.size();
    size_t theSize = static_cast<size_t>(size);
    assert(length <= theSize);
    
    memcpy(msg, poppedVector, length); // converted data to char* via vector::data()

    // 4. Wake up threads that were waiting for pop
    slot_available.notify_one();

    // 5. Return the vector's length to the caller so that they know how many bytes were popped
    return length;
}

size_t BoundedBuffer::size () {
    return q.size();
}
