// This needs to be defined before anything is included in order to get
// the PRIx64 macro
// #define __STDC_FORMAT_MACROS

#include "stdio.h"
#include <algorithm>
#include <iostream>
#include <map>
#include <tuple>
#include <vector>

#include "panda/plugin.h"

extern "C" {
bool init_plugin(void *);
void uninit_plugin(void *);
int virt_mem_after_read(CPUState *env, target_ulong pc, target_ulong addr,
                        target_ulong size, void *buf);
}

struct AddressStatus {
    int i;
    int k;
    int k_prev;
    bool direction;
    uint64_t instruction_start;
};

std::map<target_ulong, AddressStatus> addresses;
// int delay = 500000000;

bool init_plugin(void *self) {
    printf("[PT5] Hi!\n");
    panda_enable_memcb();
    panda_cb pcb;
    pcb.virt_mem_after_read = virt_mem_after_read;
    panda_register_callback(self, PANDA_CB_VIRT_MEM_AFTER_READ, pcb);
    return true;
}

void uninit_plugin(void *self) {
    for (auto it = addresses.begin(); it != addresses.end(); it++) {
        auto status = it->second;
        if (status.i > 7) {
            printf("%" PRIx64 " started at %" PRIu64 "\n", it->first,
                   status.instruction_start);
        }
    }
    printf("[PT5] Bye bye!\n");
}

int virt_mem_after_read(CPUState *env, target_ulong pc, target_ulong addr,
                        target_ulong size, void *buf) {
    // if (delay > 0) {
    //     delay--;
    //     return 0;
    // } else if (delay == 0) {
    //     delay = -1;
    //     printf("PT5 START\n");
    // }

    int notes[] = {74, 76, 78, 79, 81, 83, 85, 86};
    auto address_status_iter = addresses.find(addr);
    uint8_t byte = *(uint8_t *)buf;
    if (address_status_iter != addresses.end()) {
        AddressStatus address_status = address_status_iter->second;
        if (byte == notes[address_status.k]) {
            int i = address_status.i;
            int k = address_status.k;
            int k_prev = k;
            bool direction = address_status.direction;

            if (k == 0) {
                direction = true;
            } else if (k == 7) {
                direction = false;
            }
            if (direction) {
                k++;
            } else {
                k--;
            }

            addresses[addr] = AddressStatus{
                i + 1, k, k_prev, direction, address_status.instruction_start,
            };

            if (i >= 7) {
                printf("INTERESTING: %" PRIx64 ": %d - %d\n", addr, i, k_prev);
            }
        } else if (byte == notes[address_status.k_prev]) {
            // Do nothing
        } else {
            addresses.erase(addr);
            if (address_status.i > 7) {
                printf("LOST INTERESTING: %" PRIx64
                       " expected %d but got %d.\n",
                       addr, address_status.k, byte);
            }
        }
    } else if (byte == notes[0]) {
        // new candidate
        addresses[addr] = AddressStatus{
            1, 1, 0, true, env->rr_guest_instr_count,
        };
    }
    return 0;
}
