#ifndef TETRANE_RING3_H_
#define TETRANE_RING3_H_

# include <iostream>
# include <VBox/vmm/hm.h>

//! Notifies our tweaks of a port read.
void read_port(PVM pVM, RTIOPORT Port, uint32_t read_value, size_t cbValue);

//! Notifies our tweaks of a port write.
void write_port(PVM pVM, RTIOPORT Port, uint32_t written_value, size_t cbValue);

void tetrane_init_tweaks(PVM pVM);

#endif
