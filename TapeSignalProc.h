#ifndef TAPESIGNALPROC_H
#define TAPESIGNALPROC_H

/*
 * Adaptive tape comparator.
 *
 * Threshold is halfway between the minimum and maximum of approximately
 * the last N samples. Monotonic queues make min/max O(1) amortized instead
 * of scanning all 256 samples for every WAV sample. This matters on the 486
 * target.
 */
class TapeSignalProc {
public:
    TapeSignalProc(int size);
    ~TapeSignalProc();

    bool addSample(int sample);

private:
    struct QueueItem {
        int value;
        unsigned long index;
    };

    static const int INIT;

    void expireOld(unsigned long index);
    void pushMin(int value, unsigned long index);
    void pushMax(int value, unsigned long index);

    int windowSize;
    int capacity;
    unsigned long sequence;

    QueueItem* minQueue;
    QueueItem* maxQueue;
    int minHead;
    int minTail;
    int minCount;
    int maxHead;
    int maxTail;
    int maxCount;
};

#endif // TAPESIGNALPROC_H
