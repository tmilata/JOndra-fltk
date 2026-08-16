#include "TapeSignalProc.h"

const int TapeSignalProc::INIT = 128;

TapeSignalProc::TapeSignalProc(int size)
    : windowSize(size > 0 ? size : 1),
      capacity((size > 0 ? size : 1) + 1),
      sequence(1),
      minQueue(0), maxQueue(0),
      minHead(0), minTail(0), minCount(0),
      maxHead(0), maxTail(0), maxCount(0)
{
    minQueue = new QueueItem[capacity];
    maxQueue = new QueueItem[capacity];

    // Start the sample window at midpoint 128. One queue item is enough to
    // represent the initial min/max until real samples replace it.
    minQueue[0].value = INIT;
    minQueue[0].index = 0;
    minTail = 1;
    minCount = 1;

    maxQueue[0].value = INIT;
    maxQueue[0].index = 0;
    maxTail = 1;
    maxCount = 1;
}

TapeSignalProc::~TapeSignalProc() {
    delete [] minQueue;
    delete [] maxQueue;
}

void TapeSignalProc::expireOld(unsigned long index) {
    while (minCount > 0 && (index - minQueue[minHead].index) >= (unsigned long)windowSize) {
        minHead = (minHead + 1) % capacity;
        --minCount;
    }
    while (maxCount > 0 && (index - maxQueue[maxHead].index) >= (unsigned long)windowSize) {
        maxHead = (maxHead + 1) % capacity;
        --maxCount;
    }
}

void TapeSignalProc::pushMin(int value, unsigned long index) {
    while (minCount > 0) {
        int back = (minTail + capacity - 1) % capacity;
        if (minQueue[back].value < value) break;
        minTail = back;
        --minCount;
    }
    minQueue[minTail].value = value;
    minQueue[minTail].index = index;
    minTail = (minTail + 1) % capacity;
    ++minCount;
}

void TapeSignalProc::pushMax(int value, unsigned long index) {
    while (maxCount > 0) {
        int back = (maxTail + capacity - 1) % capacity;
        if (maxQueue[back].value > value) break;
        maxTail = back;
        --maxCount;
    }
    maxQueue[maxTail].value = value;
    maxQueue[maxTail].index = index;
    maxTail = (maxTail + 1) % capacity;
    ++maxCount;
}

bool TapeSignalProc::addSample(int sample) {
    unsigned long index = sequence++;

    expireOld(index);
    pushMin(sample, index);
    pushMax(sample, index);

    // The queues are never empty after the pushes above.
    int minValue = minQueue[minHead].value;
    int maxValue = maxQueue[maxHead].value;
    int threshold = minValue + (maxValue - minValue) / 2;

    return sample > threshold;
}
