#include <camkes.h>
#include <stdio.h>
#include "CMASI/lmcp.h"
#include "WaypointManagerUtils.h"
#include "../../../../../components/VM/src/vm.h"
#define MAX_AP_WAYPOINTS 8
#define DEBUG(fmt,args...)  ;//  printf("%s,%s,%i:"fmt,__FUNCTION__,"waypoint_manager.c",__LINE__,##args)

#define UART_PACKET_SZ 255
#define WIN_SZ 8


static lmcp_object * mc = NULL;
static Waypoint * mcwps = NULL;
static uint32_t mcwpslen = 0;
static MissionCommand win;
static Waypoint winwps[WIN_SZ];

void mission_command_ready() {
  
  uint8_t * tmp_mission = (uint8_t*)mission_command;
  MissionCommand * local_mc = NULL;
  unsigned int i;
  
  if(mc != NULL) {
    lmcp_free((lmcp_object*)mc);
    mc = NULL;
  }

  if(mcwps != NULL) {
    free(mcwps);
    mcwps = NULL;
    mcwpslen = 0;
  }

  int err = lmcp_process_msg((uint8_t **)&tmp_mission,MISSION_COMMAND_SIZE,&mc);

  if(mc->type != 36 || err == -1) {
    printf("WM: Bad mission command!\n");
    lmcp_free(mc);
    mc = NULL;
    return;
  }

  lmcp_pp(mc);
  
  mcwpslen = ((MissionCommand*)mc)->WaypointList_ai.length;
  mcwps = (Waypoint*)malloc(sizeof(Waypoint)*mcwpslen);
  for(i = 0; i < mcwpslen; i++) {
    mcwps[i] = *(((MissionCommand*)mc)->WaypointList[i]);
  }

  win = *((MissionCommand*)mc);
  win.WaypointList_ai.length = WIN_SZ;
  
  assert(
	 AutoPilotMissionCommandSegment
	 (
	  mcwps,
	  mcwpslen,
	  ((MissionCommand*)mc)->FirstWaypoint,
	  winwps,
	  WIN_SZ
	 )
	 == true
	 );

  printf("WM: Completed call to AutoPilotMissionCommandSegment!\n");

  lmcp_pp((lmcp_object*)&win);
  
   /*assert(MCWaypointSubSequence(local_mc,
                               local_mc->FirstWaypoint,
                               MAX_AP_WAYPOINTS,
                               &swin) == true);  
  
  missionSendState = true;
  sendUartPacket = true;*/
  return;
}

void pre_init(void) {

    unsigned int i;
    win.WaypointList = (Waypoint**)malloc(sizeof(Waypoint*)*WIN_SZ);
    for(i = 0; i < WIN_SZ; i++) {
      win.WaypointList[i] = &(winwps[i]);
    }
    return;
}
    

int run(void) {
  
  while (1) {
    give_mission_command_wait();

    printf("WM: Received mission command.\n");
    mission_command_ready();
    

    got_mission_command_emit();    
  }

  return 0;
}

