#ifndef TETRANE_DEVICE_H
#define TETRANE_DEVICE_H

#include "stream_file.h"

struct port_range
{
    std::string description;
    uint16_t port;
    uint16_t length;
    uint32_t instance;
};

inline stream_file& operator<<(stream_file&out, const port_range& port)
{
    out << port.description << port.port << port.length << port.instance;
    return out;
}



struct memory_range
{
    std::string description;
    uint64_t physical_address;
    uint64_t length;
    uint32_t instance;
};

inline stream_file& operator<<(stream_file&out, const memory_range& mmio)
{
    out << mmio.description << mmio.physical_address << mmio.length << mmio.instance;
    return out;
}



struct device
{
    std::string name;
    std::string description;
    uint64_t id;
    std::vector<port_range> port_ranges;
    std::vector<memory_range> memory_ranges;
};


inline stream_file& operator<<(stream_file&out, const device& dev)
{
    out << dev.name << dev.description << dev.id << dev.port_ranges << dev.memory_ranges;
    return out;
}



#endif //TETRANE_DEVICE_H
