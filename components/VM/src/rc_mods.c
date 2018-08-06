#include <autoconf.h>

#include "vmlinux.h"

#include <string.h>

#include <vka/capops.h>
#include <camkes.h>

#include <sel4arm-vmm/vm.h>
#include <sel4arm-vmm/images.h>
#include <sel4arm-vmm/plat/devices.h>
#include <sel4arm-vmm/devices/vgic.h>
#include <sel4arm-vmm/devices/vram.h>
#include <sel4arm-vmm/devices/vusb.h>
#include <sel4utils/irq_server.h>
#include <cpio/cpio.h>

#include <sel4arm-vmm/devices/generic_forward.h>


#define STRING_READY 0xE0000000
static char string_ready = 0; 
static int handle_string_ready_fault
  (struct device* d, vm_t* vm, fault_t* fault) {

  uint32_t fdata = fault_get_data(fault);
  uint32_t faddr = fault_get_address(fault);
  uint32_t fmask = fault_get_data_mask(fault);
  printf("VM: Sending string ready notification.\n");
  str_mutex_lock();
  string_ready = 1;
  str_mutex_unlock();
  give_string_emit();
  return advance_fault(fault);  
}

const struct device dev_string_ready = {
    .devid = DEV_CUSTOM,
    .name = "string_ready",
    .pstart = STRING_READY,
    .size = PAGE_SIZE,
    .handle_page_fault = handle_string_ready_fault,
    .priv = NULL
};

#define STRING (STRING_READY+PAGE_SIZE)
static int handle_string_fault(struct device* d, vm_t* vm, fault_t* fault){
  uint32_t fdata = fault_get_data(fault);
  uint32_t faddr = fault_get_address(fault);
  uint32_t fmask = fault_get_data_mask(fault);
  if (fault_is_read(fault)) {
    fault_set_data(fault,((volatile char*)str)[faddr - STRING]);
  } else {
    str_mutex_lock();
    if(!string_ready) {
      printf("VM: Write to %x.\n",faddr);
      ((volatile char*)str)[faddr - STRING] = fdata;
      str_mutex_unlock();
    } else {
      str_mutex_unlock();
      printf("VM: Write to %x unsuccessful; waiting on got_string event.\n"
	     ,faddr);    
    }
  }
  return advance_fault(fault);
}

const struct device dev_string = {
    .devid = DEV_CUSTOM,
    .name = "string",
    .pstart = STRING,
    .size = PAGE_SIZE,
    .handle_page_fault = handle_string_fault,
    .priv = NULL
    };

static void got_string_callback(void * unused) {
  int err;
  printf("VM: String received by rev component.\n");
  str_mutex_lock();
  string_ready = 0;
  str_mutex_unlock();
  err = got_string_reg_callback(got_string_callback,NULL);
  assert(!err);

}

static char gnirts_ready = 0;
static void give_gnirts_callback(void * unused) {
  int err;
  printf("VM: Reverse string(%s) ready.\n",(char*)rts);
  rts_mutex_lock();
  gnirts_ready = 1;
  rts_mutex_unlock();
  err = give_gnirts_reg_callback(give_gnirts_callback,NULL);
  assert(!err);
}

#define GNIRTS_READY (STRING+PAGE_SIZE)
static int handle_gnirts_ready_fault(struct device* d, vm_t* vm, fault_t* fault){
  uint32_t fdata = fault_get_data(fault);
  uint32_t faddr = fault_get_address(fault);
  uint32_t fmask = fault_get_data_mask(fault);
  if (fault_is_write(fault)) {
    printf("VM: Ignoring write to %x.\n",faddr);
  } else {
    rts_mutex_lock();
    printf("VM: Read to %x.\n",faddr);
    fault_set_data(fault,gnirts_ready);
    rts_mutex_unlock();
  }
  return advance_fault(fault);
}

const struct device dev_gnirts_ready = {
    .devid = DEV_CUSTOM,
    .name = "gnirts_ready",
    .pstart = GNIRTS_READY,
    .size = PAGE_SIZE,
    .handle_page_fault = handle_gnirts_ready_fault,
    .priv = NULL
    };

#define GNIRTS (GNIRTS_READY+PAGE_SIZE)
static int handle_gnirts_fault(struct device* d, vm_t* vm, fault_t* fault){
  uint32_t fdata = fault_get_data(fault);
  uint32_t faddr = fault_get_address(fault);
  uint32_t fmask = fault_get_data_mask(fault);
  
  if (fault_is_write(fault) || fault_is_prefetch(fault)) {
    printf("VM: Ignoring write to %x.\n",faddr);
    return ignore_fault(fault);
  } else if(fault_is_read(fault)){
    rts_mutex_lock();
    if(gnirts_ready) {
      char tmp = ((char*)rts)[faddr - GNIRTS];
      /* NB: Loads on word boundaries even when loading bytes. I may
       * be indexing into a particular byte in that word. Therefore,
       * when I am setting data after a fault I must ensure that the
       * data I would like to return for that byte is in the correct
       * position in the word.
       */
      fdata = fmask
	    & (tmp << 24 | tmp << 16 | tmp << 8 | tmp);
      printf("VM: Read of %x(rts[%u] = %c).\n",faddr,faddr -
	     GNIRTS,tmp);
      fault_set_data(fault,fdata);
      rts_mutex_unlock();
    }
    else {
      rts_mutex_unlock();
      printf("VM: Read of %x unsuccessful; gnirts_ready != 1.\n"
	     ,faddr);
      fault_set_data(fault, 0);
    }
    return advance_fault(fault); }
}

const struct device dev_gnirts = {
    .devid = DEV_CUSTOM,
    .name = "gnirts",
    .pstart = GNIRTS,
    .size = PAGE_SIZE,
    .handle_page_fault = handle_gnirts_fault,
    .priv = NULL
    };

#define GNIRTS_DONE (GNIRTS+PAGE_SIZE)
static int handle_gnirts_done_fault
  (struct device* d, vm_t* vm, fault_t* fault) {
  printf("VM: Reverse string received.\n");
  rts_mutex_lock();
  gnirts_ready = 0;
  rts_mutex_unlock();
  got_gnirts_emit();
  return advance_fault(fault);  
}

const struct device dev_gnirts_done = {
    .devid = DEV_CUSTOM,
    .name = "gnirts_done",
    .pstart = GNIRTS_DONE,
    .size = PAGE_SIZE,
    .handle_page_fault = handle_gnirts_done_fault,
    .priv = NULL
};



void install_rc_linux_devices(vm_t* vm) {

  int err;
  
  err = vm_add_device(vm, &dev_string_ready);
  assert(!err);
  err = vm_add_device(vm, &dev_string);
  assert(!err);
  err = got_string_reg_callback(got_string_callback,NULL);
  assert(!err);
  err = give_gnirts_reg_callback(give_gnirts_callback,NULL);
  assert(!err);
  err = vm_add_device(vm, &dev_gnirts_ready);
  assert(!err);
  err = vm_add_device(vm, &dev_gnirts);
  assert(!err);
  err = vm_add_device(vm, &dev_gnirts_done);
  assert(!err);

  return;
}
