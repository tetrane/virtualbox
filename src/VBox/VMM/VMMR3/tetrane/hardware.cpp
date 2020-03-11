#include <VBox/vmm/tetrane.h>
#include <VBox/vmm/uvm.h>

#include <sstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <iomanip>
#include <iostream>
#include <cstdlib>

#include "vm_tweaks.h"

#define HARDWARE_FILE_MAGIC 0x68636e79734e5652

using tetrane::tweaks::vm_tweaks;

void vm_tweaks::open_hardware_file()
{
    std::string hardware_file_path = work_folder + "hardware.bin";

    hardware_file = new stream_file;
    hardware_file->open(hardware_file_path);

    uint64_t magic = HARDWARE_FILE_MAGIC;
    uint32_t version = 0;

    *hardware_file << magic << version;
}

void vm_tweaks::register_hardware_access_priv(struct saved_hardware_access *access, const unsigned char *data)
{
    plugin_manager_.hardware_access_callback(access, data);

    if (not is_dumping_io())
        return;

    *hardware_file << access->tsc
                   << access->address
                   << access->type
                   << access->device_reg
                   << access->device_instance
                   << access->length;

    hardware_file->raw_write(reinterpret_cast<const char*>(data), access->length);

    // Mark the access as being free again.
    ASMAtomicWriteU64(&access->tsc, 0);
}

void vm_tweaks::register_pending_hardware_accesses()
{
    uint32_t last_write = ASMAtomicReadU32(&cpu->reven.hardware_write_index);

    uint32_t cur_read;

    while ((cur_read = ASMAtomicReadU32(&cpu->reven.hardware_read_index)) != last_write
           && cpu->reven.hardware_access[cur_read].tsc ) {
        uint32_t i = atomic_increment(&cpu->reven.hardware_read_index, 1, HARDWARE_COUNT);

        AssertMsg(i < HARDWARE_COUNT, ("Tried to access an invalid hardware access: %d/%d",i, HARDWARE_COUNT));

        saved_hardware_access& access = cpu->reven.hardware_access[i];

        // ensure that the access has been fully written
        AssertMsg(access.tsc != 0, ("Tried to access an invalid context: %d/%d, last_write=%d", i,
                                    HARDWARE_COUNT,last_write));

        register_hardware_access_priv(&access, cpu->reven.hardware_data + access.data_start);
    }
}

void vm_tweaks::register_hardware_access(struct saved_hardware_access *access, const unsigned char *data)
{
    if (not is_dumping_io())
        return;

    RTSemMutexRequest(hardware_mutex, RT_INDEFINITE_WAIT);

    register_pending_hardware_accesses();

    register_hardware_access_priv(access, data);

    RTSemMutexRelease(hardware_mutex);
}
