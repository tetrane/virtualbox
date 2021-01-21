#include <VBox/types.h>
#include <VBox/vmm/vmcc.h>
#include <VBox/vmm/vm.h>
#include <VBox/vmm/tm.h>
#include <VBox/vmm/pdmdev.h>

#ifndef TETRANE_H_
#define TETRANE_H_

#ifdef IN_RING3
# include "tetrane_ring3.h"
#endif // IN_RING3

namespace hardware_access_type
{
enum hardware_access_type
{
    write = 1,
    pci = 2,
    mmio = 4,
    port = 8
};
}

/** The following functions allow to store the sync points.
 *
 * Note that save_sync_point is mostly used in ring0 context, but not exclusively.
 * For example save_sync_point can be used in debug mode (R3)
 */


/**
 * Detect if the sync point buffer is almost full.
 *
 * This allows the ring0 handler to return to ring3 as soon as possible to fix the issue.
 *
 */
inline bool is_sync_point_full(PVMCC pVM, PVMCPUCC pVCpu)
{
    if (pVCpu == NULL)
        pVCpu = VMCC_GET_CPU_0(pVM);
    if (!pVCpu->reven.dumping)
        return false;

    uint32_t write_index = ASMAtomicReadU32(&pVCpu->reven.sync_point_write_index);
    uint32_t read_index = ASMAtomicReadU32(&pVCpu->reven.sync_point_read_index);

    uint32_t next_index = write_index;

    if (next_index < read_index)
        next_index += SYNC_POINT_COUNT;

    uint32_t diff = next_index - read_index;

    return diff > SYNC_POINT_COUNT - 20;
}

/**
 * Spin lock to increment the following value with the specified max value.
 */
inline uint32_t atomic_increment(volatile uint32_t* value, uint32_t inc, uint32_t max)
{
    uint32_t next;
    uint32_t ret;

    do {
        ret = ASMAtomicReadU32(value);

        next = ret + inc;
        if (next >= max)
            next = 0;

        ASMNopPause();
    } while (!RT_LIKELY(ASMAtomicCmpXchgU32(value, next, ret)));

    return ret;
}

/**
 * Save a context and return a pointer to the saved context.
 *
 * This returns NULL if we're not currently recording anything.
 */
inline struct sync_point *save_sync_point_int(PVMCC pVM, PVMCPUCC pVCpu, PCPUMCTX pCtx, bool is_irq, uint8_t exit_reason,
                                              uint32_t fault_error_code)
{
    if (pVCpu == NULL)
        pVCpu = VMCC_GET_CPU_0(pVM);
    if (!pVCpu->reven.dumping)
        return NULL;

    uint32_t index = atomic_increment(&pVCpu->reven.sync_point_write_index, 1, SYNC_POINT_COUNT);
    AssertMsg(index < SYNC_POINT_COUNT, ("Tried to access an invalid context: %d/%d", index, SYNC_POINT_COUNT));

    // Note: the buffer should never be full, because we return to ring3 to flush it before it does
    AssertMsg((index + 1 ) % SYNC_POINT_COUNT != pVCpu->reven.sync_point_read_index,
              ("No room for buffer at W=%d, R=%d, max=%d\n", index,
               pVCpu->reven.sync_point_read_index, SYNC_POINT_COUNT));

    struct sync_point *sync_point = pVCpu->reven.sync_point + index;

    uint64_t tsc = ASMAtomicReadU64(&sync_point->tsc);
    AssertMsg(!tsc, ("Context number %d is already in use (TSC=0x%X)'\n", index, tsc));

    sync_point->rax = pCtx->rax;
    sync_point->rbx = pCtx->rbx;
    sync_point->rcx = pCtx->rcx;
    sync_point->rdx = pCtx->rdx;

    sync_point->rsi = pCtx->rsi;
    sync_point->rdi = pCtx->rdi;

    sync_point->rbp = pCtx->rbp;
    sync_point->rsp = pCtx->rsp;

    sync_point->r8 = pCtx->r8;
    sync_point->r9 = pCtx->r9;
    sync_point->r10 = pCtx->r10;
    sync_point->r11 = pCtx->r11;
    sync_point->r12 = pCtx->r12;
    sync_point->r13 = pCtx->r13;
    sync_point->r14 = pCtx->r14;
    sync_point->r15 = pCtx->r15;

    sync_point->rip = pCtx->rip;

    sync_point->rflags = *(uint64_t*)(&pCtx->rflags);

    sync_point->cr0 = pCtx->cr0;
    sync_point->cr2 = pCtx->cr2;
    sync_point->cr3 = pCtx->cr3;
    sync_point->cr4 = pCtx->cr4;

    PX86XSAVEAREA pXState = (PX86XSAVEAREA)pCtx->CTX_SUFF(pXState);
    sync_point->fpu_sw = pXState->x87.FSW;
    sync_point->fpu_cw = pXState->x87.FCW;
    sync_point->fpu_tw = pXState->x87.FTW & 0xff; // Abridged tags

    sync_point->type_raw = 0;
    sync_point->type.vmexit_reason = exit_reason;
    sync_point->type.is_irq = is_irq;

    sync_point->cs = pCtx->cs.Sel;

    sync_point->fault_error_code = fault_error_code;

    // last field to write, tsc != 0 indicates that the sync point is now fully written
    sync_point->tsc = TMCpuTickGet(pVCpu);

    AssertMsg(sync_point->tsc != 0, ("TSC went back to 0, this will cause issues"));

    return sync_point;
}

/**
 * Save a context and return a pointer to the saved context.
 *
 * This returns NULL if we're not currently recording anything.
 */
inline struct sync_point *save_sync_point(PVMCC pVM, PVMCPUCC pVCpu, PCPUMCTX pCtx, uint8_t exit_reason)
{
    return save_sync_point_int(pVM, pVCpu, pCtx, false, exit_reason, 0);
}


/**
 * Save an IRQ context.
 *
 * This mostly call save_sync_point with the right arguments.
 */
inline struct sync_point *save_irq(PVMCC pVM, PVMCPUCC pVCpu, PCPUMCTX pCtx, uint8_t ivt, uint32_t fault_error_code,
                                   uint64_t fault_address)
{
    uint64_t const old_cr2 = pCtx->cr2;
    if (ivt == X86_XCPT_PF) {
        pCtx->cr2 = fault_address;
    }
    sync_point *sp = save_sync_point_int(pVM, pVCpu, pCtx, true, ivt, fault_error_code);
    pCtx->cr2 = old_cr2;
    return sp;
}

/**
 * The following functions allow to store hardware accesses in a temporary buffer
 * As soon as we get back in ring3 (next sync point that happens in ring3) we store all the
 * received events into the file.
 *
 * If we're already in ring3, we avoid storing the data since we can reuse the buffer (this
 * helps because we sometimes have big buffers in ring3, and they wouldn't fit in the data field)
 *
 *
 */


#ifndef IN_RING3

/*
 * Returns an hardware access object available for storage.
 * returns NULL if we're not currently saving anything or if no access is available.
 *
 * Note: in ring3, we allocate a new access on the heap because this avoids using the ring0 buffers
 */
inline struct saved_hardware_access *find_hardware_access(PVMCC pVM, PVMCPUCC pVCpu, uint32_t length)
{
    struct saved_hardware_access *access;

    uint32_t index = atomic_increment(&pVCpu->reven.hardware_write_index, 1, HARDWARE_COUNT);

    if ((index+1) % HARDWARE_COUNT == pVCpu->reven.hardware_read_index)
        return NULL;

    // Note: currently we have no way to prevent this from happening, but it's extremely unlikely
    // because the sync point buffer is small.
    AssertMsg((index+1) % HARDWARE_COUNT != pVCpu->reven.hardware_read_index,
              ("No room for hardware buffer at TSC 0x%X length 0x%X\n", TMCpuTickGet(pVCpu), length));

    AssertMsg(index < HARDWARE_COUNT, ("Tried to access an invalid hardware access: %d/%d", index, HARDWARE_COUNT));

    access = pVCpu->reven.hardware_access + index;

    uint64_t tsc = ASMAtomicReadU64(&access->tsc);
    if (tsc)
        return NULL;
    AssertMsg(!tsc, ("Hardware access number %d is already in use (TSC=0x%X)'\n", index, tsc));

    access->data_start = atomic_increment(&pVCpu->reven.hardware_cur_data, length, sizeof(pVCpu->reven.hardware_data));
    access->length = length;

    return access;
}

#endif

/**
 * Save an hardware access.
 *
 * In Ring0, the data is transient  so it must be stored for later use.
 * In Ring3, we know that the hardware access will be stored immediately on disk, so there is no need to store it.
 *
 * This returns the saved access in case it needs to be modified. Returns NULL if not dumping or no room is available.
 */
inline VBOXSTRICTRC save_hardware_access(PVMCC pVM, PVMCPUCC pVCpu,
    PPDMDEVINS pDevIns,
    uint64_t address, unsigned int length, const uint8_t *data, uint64_t type)
{
    if (pVCpu == NULL)
        pVCpu = VMCC_GET_CPU_0(pVM);
    if (!pVCpu->reven.dumping)
        return VINF_SUCCESS;

    struct saved_hardware_access *access;

#ifdef IN_RING3
    access = new saved_hardware_access;
    memset(access, 0, sizeof(*access));
#else
    access = find_hardware_access(pVM, pVCpu, length);
    if (!access)
        return VINF_EM_RAW_TO_R3;
#endif

    access->address = address;
    access->length = length;
    access->type = type;

    if (pDevIns) {
        access->device_reg = (uint64_t)pDevIns->pReg;
        access->device_instance = pDevIns->iInstance;
    } else {
        access->device_reg = 0;
        access->device_instance = 0;
    }

#ifdef IN_RING3
    // In Ring3, directly send the data to the file.
    // This avoid storing too much data in the cpu when it's not actually needed.
    access->tsc = TMCpuTickGet(pVCpu);
    register_hardware_access(pVM, access, data);
    delete access;
#else
    memcpy(pVCpu->reven.hardware_data + access->data_start, data, length);
    ASMAtomicWriteU64(&access->tsc, TMCpuTickGet(pVCpu));
#endif

    return VINF_SUCCESS;
}


/**
 * Save an MMIO access.
 *
 * This mostly calls save_hardware_access with the right flags.
 *
 * Note that the read/write semantics of MMIO accesses are reversed because we're storing the device point of view.
 * This allows to apply the access in Reven cleanly (an MMIO read in Reven happens after an MMIO write hardware access)
 */
inline VBOXSTRICTRC save_mmio_access(PVMCC pVM, PVMCPUCC pVCpu,
    PPDMDEVINS pDevIns,
    uint64_t address, unsigned int length, const uint8_t *data, bool write)
{
    uint64_t type = hardware_access_type::mmio;
    // an MMIO read is like the device writing in main memory,
    // and a mmio write is like the device reading the main memory
    if (!write)
        type |= hardware_access_type::write;

    return save_hardware_access(pVM, pVCpu, pDevIns, address, length, data, type);
}

/**
 * Save a PCI access.
 *
 * This mostly calls save_hardware_access with the proper flags.
 */
inline VBOXSTRICTRC save_pci_access(PVMCC pVM, PVMCPUCC pVCpu,
    PPDMDEVINS pDevIns,
    uint64_t address, unsigned int length, const uint8_t *data, bool write)
{
    uint64_t type = hardware_access_type::pci;
    if (write)
        type |= hardware_access_type::write;

    return save_hardware_access(pVM, pVCpu, pDevIns, address, length, data, type);
}

/**
 * Save a port access.
 *
 * This stores a port string access.
 * Regular port accesses are not stored because we can just rely on the sync point to retrieve the value.
 *
 * Unlike the other hardware accesses, the address is not a physical address but a linear address.
 * Also, we don't have the content at the time of the access, which means that we need to read the values
 * from the guest RAM.
 *
 * Once we have the data, we store it manually, i.e. we duplicate some things that can be found in
 * save_hardware_access.
 */
inline VBOXSTRICTRC save_port_access(PVMCC pVM, PVMCPUCC pVCpu,
    RTGCPTR address, unsigned int length, bool write)
{
    if (pVCpu == NULL)
        pVCpu = VMCC_GET_CPU_0(pVM);
    if (pVCpu->reven.dumping == 0)
        return VINF_SUCCESS;
    uint64_t type = hardware_access_type::port;
    // a port read is like the device writing in main memory,
    // and a port write is like the device reading the main memory
    if (!write)
        type |= hardware_access_type::write;

    // This is only used to temporarily store the data
    saved_hardware_access *access;
    VBOXSTRICTRC rc;
    uint8_t *u8data;

#ifdef IN_RING3
    access = new saved_hardware_access;
    memset(access, 0, sizeof(*access));
    u8data = new uint8_t[length];
#else
    access = find_hardware_access(pVM, pVCpu, length);
    if (!access)
        return VINF_EM_RAW_TO_R3;
    u8data = pVCpu->reven.hardware_data + access->data_start;
#endif

    access->address = address;
    access->length = length;
    access->type = type;

#ifdef IN_RC
    rc = MMGCRamReadNoTrapHandler(u8data, (void *)(uintptr_t)address, length);
#else // IN_RING0 or IN_RING3
    rc = PGMPhysReadGCPtr(pVCpu, u8data, address, length, PGMACCESSORIGIN_IOM);
#endif

    if (rc != VINF_SUCCESS)
    {
        // This should never happen since we just wrote / read it during the port read / write.
        AssertMsgFailed(("Cannot read for port string for length %d at %x\n", length, address));
        return rc;
    }

    ASMAtomicWriteU64(&access->tsc, TMCpuTickGet(pVCpu));
#ifdef IN_RING3
    register_hardware_access(pVM, access, u8data);
    delete access;
#endif

    return VINF_SUCCESS;
}


# endif // !TETRANE_H_
