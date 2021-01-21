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

#include <VBox/vmm/hm_vmx.h>

#define DUMP_SIZE 4096

namespace tetrane {
namespace tweaks {

struct sync_point_header
{
    uint64_t magic;

    uint32_t version;
    uint32_t vbox_version;

    uint32_t sync_point_size;

    /* Header is 1024 bytes long */
    uint8_t padding[1024 - (8+4+4+4)];
};

enum chunk_type
{
    chunk_stop = 0, // marks the end of the chunk list

    logical_memory,
    physical_memory
    // add more types if desired.
};

void vm_tweaks::stop_hardware_monitoring()
{
    if (sync_point_file) {
        sync_point_file->close();
        delete sync_point_file;
        sync_point_file = NULL;
    }

    if (sync_point_data_file) {
        sync_point_data_file->close();
        delete sync_point_data_file;
        sync_point_data_file = NULL;
    }

    if (hardware_file) {
        RTSemMutexRequest(hardware_mutex, RT_INDEFINITE_WAIT);
        hardware_file->close();
        delete hardware_file;
        hardware_file = NULL;
        RTSemMutexRelease(hardware_mutex);
        RTSemMutexDestroy(hardware_mutex);
    }
}

void vm_tweaks::open_time_file()
{
    std::string sync_file_path = work_folder + "sync_point.bin";
    std::string sync_data_file_path = work_folder + "sync_point_data.bin";

    sync_point_file = new stream_file;
    sync_point_file->open(sync_file_path);

    sync_point_header header;

    header.magic = 0x70636e79734e5652;      // RVNsyncp
    header.version = 3;
    header.vbox_version = 0;
    header.sync_point_size = SYNC_POINT_BYTES; // not sizeof(sync_point) here
    memset(header.padding, 0, sizeof(header.padding));

    // TODO write a header in the file with some background data (file version, vm, date, rvn version…)
    sync_point_file->raw_write(reinterpret_cast<char*>(&header), sizeof(header));


    uint64_t magic_spdata = 0x64636e79734e5652;  // RVNsyncd

    sync_point_data_file = new stream_file;
    sync_point_data_file->open(sync_data_file_path);

    *sync_point_data_file << magic_spdata;

    // data cursor is currently just after the magic
    sync_point_data_pos = sizeof(magic_spdata);
}

uint32_t vm_tweaks::write_logical_chunk(uint64_t offset, uint64_t size)
{
    char mem[DUMP_SIZE];

    uint32_t chunk_type = logical_memory;

    AssertMsg(size <= DUMP_SIZE, ("buffer size not large enough: 0x%x < 0x%x", DUMP_SIZE, size));

    int rc = PGMPhysSimpleReadGCPtr(cpu, mem, offset, size);
    if (RT_FAILURE(rc))
        return 0;

    *sync_point_data_file << chunk_type << offset << size;

    sync_point_data_file->raw_write(mem, size);

    sync_point_data_pos += 4 + 8 + 8 + size;

    return 1;
}

uint32_t vm_tweaks::save_sync_point_mem_chunk(uint64_t first, uint64_t last)
{
    uint32_t num_chunks = 0;

    if (first >> 12 == last >> 12) {
        /* all in one page */

        num_chunks += write_logical_chunk(first, DUMP_SIZE);
    } else if ((first >> 12) + 1 == last >> 12) {
        /* Two consecutive pages */

        uint32_t start_size = DUMP_SIZE - (first&0xfff);

        num_chunks += write_logical_chunk(first, start_size);
        num_chunks += write_logical_chunk(first+start_size, (last&0xfff)+1);
    } else {
        /* At the moment, more than two consecutive pages is not handled.
         * This is not really an issue if this needs to be done: basically either we can read the whole buffer and
         * fail if we cannot read some byte from it, or split it at each page.
         *
         * The 2 pages case above is split because we want to be able to
         */
        fprintf(stderr, "Cannot dump memory in more than 2 pages (from 0x%lx to 0x%lx)\n", first, last);
    }
    return num_chunks;

}

uint32_t vm_tweaks::save_sync_point_mem(const sync_point& sp, bool dump_mem)
{
    if (!dump_mem)
        return 0;

    uint32_t chunk_pos = 0;
    uint32_t chunk_pos_start = sync_point_data_pos;
    uint32_t num_chunks = 0;
    RTGCPTR first, last;

    // Note: this function assumes that DUMP_SIZE == 4096.
    // It should be updated accordingly if the dump size is something else.

    AssertMsg(DUMP_SIZE == 4096, ("DUMP_SIZE=%u != 4096", DUMP_SIZE));

    // Dump the stack
    first = (RTGCPTR)(sp.rsp - DUMP_SIZE/2);
    last = (RTGCPTR)(sp.rsp + DUMP_SIZE/2-1); /* inclusive last */
    num_chunks += save_sync_point_mem_chunk(first, last);

    // Dump the user setted chunks
    for(chunk_iterator it=memory_chunks_.begin(); it != memory_chunks_.end(); ++it) {
        first = (RTGCPTR)(it->offset);
        last = (RTGCPTR)(it->offset + it->size); /* inclusive last */
        num_chunks += save_sync_point_mem_chunk(first, last);
    }

    if (num_chunks > 0) {
        // only tell the chunk position if we could read anything, otherwise 0
        chunk_pos = chunk_pos_start;

        // Mark the end of the chunk list by adding an empty chunk.
        uint32_t chunk_end = chunk_stop;
        *sync_point_data_file << chunk_end;

        sync_point_data_pos += sizeof(chunk_end);
    }

    return chunk_pos;
}

void vm_tweaks::write_sync_point(const sync_point& sp, bool dump_mem)
{
    uint16_t type_raw = 0;

    type_raw |= sp.type.interrupt_vector;
    if (sp.type.is_irq)
        type_raw |= 0x100;

    *sync_point_file
        << sp.tsc                                 // 64
        << type_raw                               // 16
        << sp.cs                                  // 16
        << sp.rax << sp.rbx << sp.rcx << sp.rdx   // 64*4
        << sp.rsi << sp.rdi                       // 64*2
        << sp.rbp << sp.rsp                       // 64*2
        << sp.r8 << sp.r9 << sp.r10 << sp.r11     // 64*4
        << sp.r12 << sp.r13 << sp.r14 << sp.r15   // 64*4
        << sp.rip << sp.rflags                    // 64*2
        << sp.cr0 << sp.cr2 << sp.cr3 << sp.cr4   // 64*4
        << save_sync_point_mem(sp, dump_mem)      // 32
        << sp.fpu_sw << sp.fpu_cw << sp.fpu_tw    // 16*2+8
        << sp.fault_error_code                    // 32
        ;
    // total:201


    // Write remaining empty bytes to make sync points the desired size
    char data[SYNC_POINT_BYTES - 201];
    memset(data, 0, sizeof(data));

    sync_point_file->raw_write(data, sizeof(data));
}

static const char *reason(uint8_t code)
{
    switch (code) {
    case VMX_EXIT_XCPT_OR_NMI:return "Exception or non-maskable interrupt (NMI)";
    case VMX_EXIT_EXT_INT: return "External interrupt.";
    case VMX_EXIT_TRIPLE_FAULT: return "Triple fault.";
    case VMX_EXIT_INIT_SIGNAL: return "INIT signal.";
    case VMX_EXIT_SIPI: return "Start-up IPI (SIPI).";
    case VMX_EXIT_IO_SMI: return "I/O system-management interrupt (SMI).";
    case VMX_EXIT_SMI: return "Other SMI.";
    case VMX_EXIT_INT_WINDOW: return "Interrupt window exiting.";
    case VMX_EXIT_NMI_WINDOW: return "NMI window exiting.";
    case VMX_EXIT_TASK_SWITCH: return "Task switch.";
    case VMX_EXIT_CPUID: return "Guest software attempted to execute CPUID.";
    case VMX_EXIT_GETSEC: return "Guest software attempted to execute GETSEC.";
    case VMX_EXIT_HLT: return "Guest software attempted to execute HLT.";
    case VMX_EXIT_INVD: return "Guest software attempted to execute INVD.";
    case VMX_EXIT_INVLPG: return "Guest software attempted to execute INVLPG.";
    case VMX_EXIT_RDPMC: return "Guest software attempted to execute RDPMC.";
    case VMX_EXIT_RDTSC: return "Guest software attempted to execute RDTSC.";
    case VMX_EXIT_RSM: return "Guest software attempted to execute RSM in SMM.";
    case VMX_EXIT_VMCALL: return "Guest software executed VMCALL.";
    case VMX_EXIT_VMCLEAR: return "Guest software executed VMCLEAR.";
    case VMX_EXIT_VMLAUNCH: return "Guest software executed VMLAUNCH.";
    case VMX_EXIT_VMPTRLD: return "Guest software executed VMPTRLD.";
    case VMX_EXIT_VMPTRST: return "Guest software executed VMPTRST.";
    case VMX_EXIT_VMREAD: return "Guest software executed VMREAD.";
    case VMX_EXIT_VMRESUME: return "Guest software executed VMRESUME.";
    case VMX_EXIT_VMWRITE: return "Guest software executed VMWRITE.";
    case VMX_EXIT_VMXOFF: return "Guest software executed VMXOFF.";
    case VMX_EXIT_VMXON: return "Guest software executed VMXON.";
    case VMX_EXIT_MOV_CRX: return "Control-register accesses.";
    case VMX_EXIT_MOV_DRX: return "Debug-register accesses.";
    case VMX_EXIT_IO_INSTR: return "I/O instruction.";
    case VMX_EXIT_RDMSR: return "RDMSR. Guest software attempted to execute RDMSR.";
    case VMX_EXIT_WRMSR: return "WRMSR. Guest software attempted to execute WRMSR.";
    case VMX_EXIT_ERR_INVALID_GUEST_STATE: return "VM-entry failure due to invalid guest state.";
    case VMX_EXIT_ERR_MSR_LOAD: return "VM-entry failure due to MSR loading.";
    case VMX_EXIT_MWAIT: return "Guest software executed MWAIT.";
    case VMX_EXIT_MTF: return "VM exit due to monitor trap flag.";
    case VMX_EXIT_MONITOR: return "Guest software attempted to execute MONITOR.";
    case VMX_EXIT_PAUSE: return "Guest software attempted to execute PAUSE.";
    case VMX_EXIT_ERR_MACHINE_CHECK: return "VM-entry failure due to machine-check.";
    case VMX_EXIT_TPR_BELOW_THRESHOLD: return "TPR below threshold. Guest software executed MOV to CR8.";
    case VMX_EXIT_APIC_ACCESS: return "APIC access. Guest software attempted to access memory at a physical address on the APIC-access page.";
    case VMX_EXIT_VIRTUALIZED_EOI: return "";
    case VMX_EXIT_GDTR_IDTR_ACCESS: return "";
    case VMX_EXIT_LDTR_TR_ACCESS: return "";
    case VMX_EXIT_EPT_VIOLATION: return "EPT violation. An attempt to access memory with a guest-physical address was disallowed by the configuration of the EPT paging structures.";
    case VMX_EXIT_EPT_MISCONFIG: return "EPT misconfiguration. An attempt to access memory with a guest-physical address encountered a misconfigured EPT paging-structure entry.";
    case VMX_EXIT_INVEPT: return "INVEPT. Guest software attempted to execute INVEPT.";
    case VMX_EXIT_RDTSCP: return "RDTSCP. Guest software attempted to execute RDTSCP.";
    case VMX_EXIT_PREEMPT_TIMER: return "VMX-preemption timer expired. The preemption timer counted down to zero.";
    case VMX_EXIT_INVVPID: return "INVVPID. Guest software attempted to execute INVVPID.";
    case VMX_EXIT_WBINVD: return "WBINVD. Guest software attempted to execute WBINVD.";
    case VMX_EXIT_XSETBV: return "XSETBV. Guest software attempted to execute XSETBV.";
    case VMX_EXIT_APIC_WRITE: return "";
    case VMX_EXIT_RDRAND: return "RDRAND. Guest software attempted to execute RDRAND.";
    case VMX_EXIT_INVPCID: return "INVPCID. Guest software attempted to execute INVPCID.";
    case VMX_EXIT_VMFUNC: return "VMFUNC. Guest software attempted to execute VMFUNC.";
    case VMX_EXIT_ENCLS: return "";
    case VMX_EXIT_RDSEED: return "";
    case VMX_EXIT_PML_FULL: return "";
    case VMX_EXIT_XSAVES: return "";
    case VMX_EXIT_XRSTORS: return "";
    case VMX_EXIT_SPP_EVENT: return "";
    case VMX_EXIT_UMWAIT: return "";
    case VMX_EXIT_TPAUSE: return "";
    }

    return "Unknown";
}

void vm_tweaks::flush_data()
{
    if (not is_dumping_io())
    {
        return;
    }

    uint32_t last_write = ASMAtomicReadU32(&cpu->reven.sync_point_write_index);
    uint32_t cur_read;

    uint64_t cur_tsc = TMCpuTickGet(cpu);

    while ((cur_read = ASMAtomicReadU32(&cpu->reven.sync_point_read_index)) != last_write
           && ASMAtomicReadU64(&cpu->reven.sync_point[cur_read].tsc) != 0) {
        uint32_t i = atomic_increment(&cpu->reven.sync_point_read_index, 1, SYNC_POINT_COUNT);
        sync_point& sync_point = cpu->reven.sync_point[i];
        bool dump_mem = false;

        AssertMsg(i < SYNC_POINT_COUNT, ("Tried to access an invalid context: %d/%d, last_write=%d", i,
                                         SYNC_POINT_COUNT,last_write));

        // ensure that the access has been fully written
        AssertMsg(sync_point.tsc != 0, ("Tried to access an invalid context: %d/%d, last_write=%d", i,
                                        SYNC_POINT_COUNT,last_write));

        if (cur_tsc == sync_point.tsc && !sync_point.type.is_irq && sync_point.type.vmexit_reason == VMX_EXIT_INT_WINDOW) {
            dump_mem = true;
        }
        write_sync_point(sync_point, dump_mem);

        // reset the access
        sync_point.tsc = 0;
    }

    RTSemMutexRequest(hardware_mutex, RT_INDEFINITE_WAIT);

    register_pending_hardware_accesses();

    RTSemMutexRelease(hardware_mutex);
}


}} //namespace tetrane::tweaks
