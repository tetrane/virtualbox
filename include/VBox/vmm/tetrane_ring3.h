#ifndef TETRANE_RING3_H_
#define TETRANE_RING3_H_

# include <iostream>
# include <VBox/vmm/hm.h>

struct IOMIOPORTRANGER3;
typedef struct IOMIOPORTRANGER3 *PIOMIOPORTRANGER3;

//! Notifies our tweaks of a port read.
void read_port(PVM pVM, RTIOPORT Port, uint32_t read_value, size_t cbValue, PIOMIOPORTRANGER3 pRange);

//! Notifies our tweaks of a port write.
void write_port(PVM pVM, RTIOPORT Port, uint32_t written_value, size_t cbValue, PIOMIOPORTRANGER3 pRange);

//! Flushes the ring0 buffers to disk.
void flush_data(PVM pVM);

void register_hardware_access(PVM pVM, struct saved_hardware_access *access, const unsigned char *data);

VBOXSTRICTRC tetrane_handle_step_instruction(PVM pVM);
VBOXSTRICTRC tetrane_handle_int3(PVM pVM);
VBOXSTRICTRC tetrane_handle_instruction_not_implemented(PVM pVM);

void tetrane_init_tweaks(PVM pVM);

#endif
