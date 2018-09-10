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

#include "vm.h"

#define MISSION_COMMAND_READY 0xE0000000
#define MISSION_COMMAND (MISSION_COMMAND_READY+PAGE_SIZE)
static char mission_command_ready = 0; 
static int handle_mission_command_ready_fault
  (struct device* d, vm_t* vm, fault_t* fault) {
  if (fault_is_read(fault)) {
    mission_command_out_mutex_lock();
    fault_set_data(fault,mission_command_ready);
    mission_command_out_mutex_unlock();
  } else {
    uint32_t fdata = fault_get_data(fault);
    uint32_t faddr = fault_get_address(fault);
    uint32_t fmask = fault_get_data_mask(fault);
    printf("VM: Sending mission command ready notification.\n");
    mission_command_out_mutex_lock();
    mission_command_ready = 1;
    mission_command_out_mutex_unlock();
    give_mission_command_emit();
  }
  return advance_fault(fault);
}

const struct device dev_mission_command_ready = {
    .devid = DEV_CUSTOM,
    .name = "mission_command_ready",
    .pstart = MISSION_COMMAND_READY,
    .size = PAGE_SIZE,
    .handle_page_fault = handle_mission_command_ready_fault,
    .priv = NULL
};


static int handle_mission_command_fault(struct device* d, vm_t* vm, fault_t* fault){
  uint32_t fdata = fault_get_data(fault);
  uint32_t faddr = fault_get_address(fault);
  uint32_t fmask = fault_get_data_mask(fault);
  if (fault_is_read(fault)) {
    fault_set_data(fault,((volatile char*)missioncommand_out)[faddr - MISSION_COMMAND]);
  } else {
    mission_command_out_mutex_lock();
    if(!mission_command_ready) {
      printf("VM: Write to %x(index = %u).\n",faddr, faddr-MISSION_COMMAND);
      ((volatile char*)missioncommand_out)[faddr - MISSION_COMMAND] = fdata;
      mission_command_out_mutex_unlock();
    } else {
      mission_command_out_mutex_unlock();
      printf("VM: Write to %x unsuccessful; waiting on got_mission_command event.\n"
	,faddr);   
    }
  }
  return advance_fault(fault);
}

const struct device dev_mission_command = {
    .devid = DEV_CUSTOM,
    .name = "mission_command",
    .pstart = MISSION_COMMAND,
    .size = MISSION_COMMAND_SIZE,
    .handle_page_fault = handle_mission_command_fault,
    .priv = NULL
    };

static void got_mission_command_callback(void * unused) {
  int err;
  printf("VM: Mission_Command received by WM component.\n");
  mission_command_out_mutex_lock();
  mission_command_ready = 0;
  mission_command_out_mutex_unlock();
  err = got_mission_command_reg_callback(got_mission_command_callback,NULL);
  assert(!err);
}


void install_rc_linux_devices(vm_t* vm) {

  int err;
  
  err = vm_add_device(vm, &dev_mission_command_ready);
  assert(!err);
  err = vm_add_device(vm, &dev_mission_command);
  assert(!err);
  err = got_mission_command_reg_callback(got_mission_command_callback,NULL);
  assert(!err);

  return;
}
