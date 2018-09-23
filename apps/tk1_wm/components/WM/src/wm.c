#include <camkes.h>
#include <stdio.h>
#include "CMASI/lmcp.h"
#include "WaypointManagerUtils.h"
#include "../../../../../components/VM/src/vm.h"
#define MAX_AP_WAYPOINTS 8
#define DEBUG(fmt,args...)  ;//  printf("%s,%s,%i:"fmt,__FUNCTION__,"waypoint_manager.c",__LINE__,##args)

#define UART_PACKET_SZ 255
#define WIN_SZ 8


#define MUTEX_OP(X) assert(X==0)
#define SEM_OP(X) assert(X==0)
#define INTR_OP(X) assert(X==0)


static lmcp_object * mc = NULL;
static Waypoint * mcwps = NULL;
static lmcp_object * airstat = NULL;
static uint32_t mcwpslen = 0;
static MissionCommand win;
static Waypoint winwps[WIN_SZ];
static uint64_t current_waypoint = 0;
static bool current_waypoint_new = false;


uint32_t Checksum(const uint8_t * p, const size_t len)
{
  uint32_t sum = 0;

  /* assumption: p is not NULL. */
  assert(p != NULL);
  
  for (size_t i = 0; i < len; i++) {
    sum += (uint32_t) p[i];
  }
  return sum;
}

void hexDump(char *desc, void *addr, int len) 
{
    int i;
    unsigned char buff[17];
    unsigned char *pc = (unsigned char*)addr;

    // Output description if given.
    if (desc != NULL)
        printf ("%s:\n", desc);

    // Process every byte in the data.
    for (i = 0; i < len; i++) {
        // Multiple of 16 means new line (with line offset).

        if ((i % 16) == 0) {
            // Just don't print ASCII for the zeroth line.
            if (i != 0)
                printf("  %s\n", buff);

            // Output the offset.
            printf("  %04x ", i);
        }

        // Now the hex code for the specific character.
        printf(" %02x", pc[i]);

        // And store a printable ASCII character for later.
        if ((pc[i] < 0x20) || (pc[i] > 0x7e)) {
            buff[i % 16] = '.';
        } else {
            buff[i % 16] = pc[i];
        }

        buff[(i % 16) + 1] = '\0';
    }

    // Pad out last line if not exactly 16 characters.
    while ((i % 16) != 0) {
        printf("   ");
        i++;
    }

    // And print the final ASCII bit.
    printf("  %s\n", buff);
}


void send_win() {
  
  UARTPacket packet;
  uint32_t netmsgsize;
  uint32_t chksum;
  uint32_t msgsize =
    lmcp_msgsize((lmcp_object*)&win)
    + sizeof(netmsgsize) /* Message length */
    + sizeof(chksum); /* checksum */
  uint32_t written = 0;

  /* Allocate memory for message. */
  uint8_t * msg = (uint8_t*)malloc(msgsize);
  memset(msg,0,msgsize);

  /* Store the full length of the message as a network order numeral
     at the begining of the message. */
  netmsgsize = htonl(msgsize);
  memcpy(msg,(uint8_t*)&netmsgsize,sizeof(netmsgsize));

  

  /* Store the message in memory after the length. */
  lmcp_make_msg(msg+sizeof(netmsgsize),(lmcp_object*)&win);

  /* Calculate checksum and store it at the end of the message. */
  chksum = Checksum(msg+sizeof(netmsgsize),
		    msgsize-(sizeof(chksum)+sizeof(netmsgsize)));
  *((uint32_t *)(msg + msgsize - sizeof(chksum))) = htonl(chksum);
  
  /*printf(
	 "WM: Sending window with a size of %u + %u (message size header)"
	 " + %u (checksum).\n"
	 , msgsize-(sizeof(chksum)+sizeof(netmsgsize))
	 , sizeof(netmsgsize)
	 , sizeof(chksum));*/

  /*hexDump("WM: msg =\n", msg, msgsize);*/
  while(written<msgsize) {
    
    memset((uint8_t*)&packet,0,sizeof(packet));
    packet.buf_len =
      (msgsize-written) < UART_PACKET_SZ ? (msgsize-written) : UART_PACKET_SZ;
    memcpy(packet.buf,msg+written,packet.buf_len);
    //printf("WM: Written %u bytes, writing %u bytes.\n",written,packet.buf_len);
    assert(sink_uartwrite(&packet)==true);
    written += packet.buf_len;
  }
  //printf("WM: Completed sending window.\n");
  free(msg);
}

void handle_mission_command() {
  
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

  mcwpslen = ((MissionCommand*)mc)->WaypointList_ai.length;
  mcwps = (Waypoint*)malloc(sizeof(Waypoint)*mcwpslen);
  for(i = 0; i < mcwpslen; i++) {
    mcwps[i] = *(((MissionCommand*)mc)->WaypointList[i]);
  }

  
  Waypoint ** tmp = win.WaypointList;
  win = *((MissionCommand*)mc);
  win.WaypointList = tmp;
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

  send_win();

  return;
}

bool mc_ready = false;

void mc_callback(void * unused) {
  //printf("WM: Received give_mission_command notification!\n");
  MUTEX_OP(mission_command_ready_lock());
  mc_ready = true;
  MUTEX_OP(mission_command_ready_unlock());
  INTR_OP(give_mission_command_reg_callback(mc_callback,NULL));
  SEM_OP(wake_run_post());
}

uint64_t nw_order(const uint64_t in) {
    unsigned char out[8] = {in>>56,in>>48,in>>40,in>>32,in>>24,in>>16,in>>8,in};
    return *(uint64_t*)out;
}


bool source_uartwrite(const UARTPacket * pkt) {
  if(pkt->buf_len < sizeof(current_waypoint)) {
    return true;
  }
  MUTEX_OP(current_waypoint_lock());
  uint64_t waypoint = *((uint64_t*)pkt->buf);
  waypoint = nw_order(waypoint);
  if(waypoint != current_waypoint) {
    //printf("WM: Got new current waypoint of %llu\n",waypoint);
    current_waypoint_new = true;
    current_waypoint = waypoint;
  }
  MUTEX_OP(current_waypoint_unlock());
  if(current_waypoint_new) { 
    SEM_OP(wake_run_post());
  }
  return true;
}

void pre_init(void) {

    unsigned int i;
    win.WaypointList = (Waypoint**)malloc(sizeof(Waypoint*)*WIN_SZ);
    for(i = 0; i < WIN_SZ; i++) {
      win.WaypointList[i] = &(winwps[i]);
    }
    win.WaypointList_ai.length = WIN_SZ;

    INTR_OP(give_mission_command_reg_callback(mc_callback,NULL));
    return;
}

int run(void) {
  
  while (1) {

    SEM_OP(wake_run_wait());

    MUTEX_OP(mission_command_ready_lock());
    if(mc_ready) {
      MUTEX_OP(mission_command_ready_unlock());
      //printf("WM: Received mission command.\n");
      handle_mission_command();
      MUTEX_OP(mission_command_ready_lock());
      mc_ready = false;
      MUTEX_OP(mission_command_ready_unlock());
      got_mission_command_emit();
    } else {
      MUTEX_OP(mission_command_ready_unlock());
    }


    MUTEX_OP(current_waypoint_lock());
    if(mc != NULL && current_waypoint_new) {
      //printf("WM: Generating new window.\n");
      current_waypoint_new = false;
      /* XXX: There is a race condition and all of the variables being
	 used with 'AutoPilotMissionCommandSegment'. It is not a problem
         currently because we are not expecting to receive more than one
         mission command.
       */
      assert(
	     AutoPilotMissionCommandSegment
	     (
	      mcwps,
	      mcwpslen,
	      current_waypoint,
	      winwps,
	      WIN_SZ
		)
	     == true
	     );
      win.FirstWaypoint = current_waypoint;
      send_win();
    }
    MUTEX_OP(current_waypoint_unlock());    
  }

  return 0;
}

