//
//  KernelUtils.c
//  Deadweight
//
//  Created by Tanay Findley on 3/13/20.
//  Copyright © 2020 Tanay Findley. All rights reserved.
//

#include "KernelUtils.h"
#include "mach_vm.h"
#include "OffsetHolder.h"
#include "find_port.h"

#define LOG(string, args...) printf(string "\n", ##args);
uint64_t task_self_addr_cache = (uint64_t)0;
mach_port_t tfp0 = MACH_PORT_NULL;
void set_tfp0(mach_port_t tfp00)
{
    tfp0 = tfp00;
}


uint64_t kmem_alloc(uint64_t size)
{
    if (tfp0 == MACH_PORT_NULL) {
        LOG("attempt to allocate kernel memory before any kernel memory write primitives available");
        return 0;
    }
    
    kern_return_t err;
    mach_vm_address_t addr = 0;
    mach_vm_size_t ksize = round_page_kernel(size);
    err = mach_vm_allocate(tfp0, &addr, ksize, VM_FLAGS_ANYWHERE);
    if (err != KERN_SUCCESS) {
        LOG("unable to allocate kernel memory via tfp0: %s %x", mach_error_string(err), err);
        return 0;
    }
    return addr;
}

bool kmem_free(uint64_t kaddr, uint64_t size)
{
    if (tfp0 == MACH_PORT_NULL) {
        LOG("attempt to deallocate kernel memory before any kernel memory write primitives available");
        return false;
    }
    
    kern_return_t err;
    mach_vm_size_t ksize = round_page_kernel(size);
    err = mach_vm_deallocate(tfp0, kaddr, ksize);
    if (err != KERN_SUCCESS) {
        LOG("unable to deallocate kernel memory via tfp0: %s %x", mach_error_string(err), err);
        return false;
    }
    return true;
}




size_t kreadOwO(uint64_t where, void* p, size_t size)
{
    int rv;
    size_t offset = 0;
    while (offset < size) {
        mach_vm_size_t sz, chunk = 2048;
        if (chunk > size - offset) {
            chunk = size - offset;
        }
        rv = mach_vm_read_overwrite(tfp0,
                                    where + offset,
                                    chunk,
                                    (mach_vm_address_t)p + offset,
                                    &sz);
        if (rv || sz == 0) {
            LOG("error reading kernel @%p", (void*)(offset + where));
            break;
        }
        offset += sz;
    }
    return offset;
}

size_t kwriteOwO(uint64_t where, const void* p, size_t size)
{
    int rv;
    size_t offset = 0;
    while (offset < size) {
        size_t chunk = 2048;
        if (chunk > size - offset) {
            chunk = size - offset;
        }
        rv = mach_vm_write(tfp0,
                           where + offset,
                           (mach_vm_offset_t)p + offset,
                           (mach_msg_type_number_t)chunk);
        if (rv) {
            LOG("error writing kernel @%p", (void*)(offset + where));
            break;
        }
        offset += chunk;
    }
    return offset;
}

bool wkbuffer(uint64_t kaddr, void* buffer, size_t length)
{
    if (tfp0 == MACH_PORT_NULL) {
        LOG("attempt to write to kernel memory before any kernel memory write primitives available");
        return false;
    }
    
    return (kwriteOwO(kaddr, buffer, length) == length);
}


bool rkbuffer(uint64_t kaddr, void* buffer, size_t length)
{
    return (kreadOwO(kaddr, buffer, length) == length);
}



uint64_t rk64_via_tfp0(uint64_t kaddr)
{
    uint64_t val = 0;
    rkbuffer(kaddr, &val, sizeof(val));
    return val;
}

uint32_t rk32_via_tfp0(uint64_t kaddr)
{
    uint32_t val = 0;
    rkbuffer(kaddr, &val, sizeof(val));
    return val;
}


uint64_t ReadKernel64(uint64_t kaddr)
{
    if (tfp0 != MACH_PORT_NULL) {
        return rk64_via_tfp0(kaddr);
    }
    
    LOG("attempt to read kernel memory but no kernel memory read primitives available");
    
    return 0;
}

uint32_t ReadKernel32(uint64_t kaddr)
{
    if (tfp0 != MACH_PORT_NULL) {
        return rk32_via_tfp0(kaddr);
    }
    
    LOG("attempt to read kernel memory but no kernel memory read primitives available");
    
    return 0;
}

void WriteKernel64(uint64_t kaddr, uint64_t val)
{
    if (tfp0 == MACH_PORT_NULL) {
        LOG("attempt to write to kernel memory before any kernel memory write primitives available");
        return;
    }
    wkbuffer(kaddr, &val, sizeof(val));
}

void WriteKernel32(uint64_t kaddr, uint32_t val)
{
    if (tfp0 == MACH_PORT_NULL) {
        LOG("attempt to write to kernel memory before any kernel memory write primitives available");
        return;
    }
    wkbuffer(kaddr, &val, sizeof(val));
}



uint64_t find_kernel_base_sockpuppet() {
    uint64_t hostport_addr = find_port_address_sockpuppet(mach_host_self(), MACH_MSG_TYPE_COPY_SEND);
    uint64_t realhost = ReadKernel64(hostport_addr + koffset(KSTRUCT_OFFSET_IPC_PORT_IP_KOBJECT));
    
    uint64_t base = realhost & ~0xfffULL;
    // walk down to find the magic:
    for (int i = 0; i < 0x10000; i++) {
        if (ReadKernel32(base) == 0xfeedfacf) {
            return base;
        }
        base -= 0x1000;
    }
    return 0;
}
