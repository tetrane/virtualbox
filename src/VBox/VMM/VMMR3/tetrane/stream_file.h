#ifndef TETRANE_STREAM_FILE_H
#define TETRANE_STREAM_FILE_H


#include <stdint.h>
#include <fstream>
#include <stdio.h>

#include <vector>
#include <string>


class stream_file
{
public:
    bool open(const std::string& path);
    void raw_write(const char *data, unsigned int size);
    ~stream_file();

    void close();
private:
    FILE *f_;
};

inline void stream_file::raw_write(const char *data, unsigned int size)
{
    fwrite(data, size, 1, f_);
}


inline stream_file& operator<<(stream_file& out, bool data)
{
    out.raw_write(reinterpret_cast<const char*>(&data), sizeof(data));
    return out;
}

inline stream_file& operator<<(stream_file& out, uint8_t data)
{
    out.raw_write(reinterpret_cast<const char*>(&data), sizeof(data));
    return out;
}

inline stream_file& operator<<(stream_file& out, uint16_t data)
{
    out.raw_write(reinterpret_cast<const char*>(&data), sizeof(data));
    return out;
}

inline stream_file& operator<<(stream_file& out, uint32_t data)
{
    out.raw_write(reinterpret_cast<const char*>(&data), sizeof(data));
    return out;
}

inline stream_file& operator<<(stream_file& out, uint64_t data)
{
    out.raw_write(reinterpret_cast<const char*>(&data), sizeof(data));
    return out;
}

inline stream_file& operator<<(stream_file& out, const std::string& data)
{
    uint64_t size = data.size();

    out << size;
    out.raw_write(reinterpret_cast<const char*>(data.c_str()), size);
    return out;
}

template<typename T>
stream_file& operator<<(stream_file& out, const std::vector<T>& vector)
{

    uint64_t size = vector.size();

    out << size;
    for (typename std::vector<T>::const_iterator it = vector.begin(); it != vector.end(); ++it)
    {
        out << *it;
    }
    return out;
}

#endif //TETRANE_STREAM_FILE_H
