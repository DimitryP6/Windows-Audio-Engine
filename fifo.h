#ifndef FIFO_H
#define FIFO_H

#include <atomic>

// The message and the FIFO from 0_control_fifo, unchanged
struct ControlMessage {
    float frequency;
    float amplitude;
    float phase;
};

constexpr unsigned int capacity = 64;

class ControlFifo {
public:
    bool push(const ControlMessage &msg)
    {
        const unsigned int write = write_ptr.load();
        const unsigned int read  = read_ptr.load();

        if (write - read == capacity)
            return false;

        slots[write % capacity] = msg;

        write_ptr.store(write + 1);

        return true;
    }

    bool pop(ControlMessage &msg)
    {
        const unsigned int read  = read_ptr.load();
        const unsigned int write = write_ptr.load();

        if (read == write)
            return false;

        msg = slots[read % capacity];

        read_ptr.store(read + 1);

        return true;
    }

private:
    ControlMessage slots[capacity];

    std::atomic<unsigned int> write_ptr = 0;
    std::atomic<unsigned int> read_ptr  = 0;
};

#endif
