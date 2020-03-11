#include "stream_file.h"

bool stream_file::open(const std::string& path)
{
    f_ = fopen(path.c_str(), "w");
    return true;
}

stream_file::~stream_file()
{
    close();
}

void stream_file::close()
{
    if (f_)
        fclose(f_);
    f_ = NULL;
}
