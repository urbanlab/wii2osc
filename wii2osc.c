/* 
 * wii2osc.c
 * $Id: wii2osc.c 5 2007-11-19 16:27:33Z leucos $
 *
 * Program converting Wiimote input to OSC
 * 
 * Copyright (C) 2007 Michel Blanc (mblanc@erasme.org) - ERASME
 * 
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/* TODO :

- daemonization
- multiple wiimotes

*/

/* Simple Makefile :

CC=gcc
CFLAGS=
LDFLAGS=-lcwiimote -lbluetooth -lm -llo
EXEC=wii2osc
SRC= $(wildcard *.c)
OBJ= $(SRC:.c=.o)

all: $(EXEC)

wii2osc.o: debug.h

wii2osc: $(OBJ)
        @echo compiling $(EXEC)
        @$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c debug.h
        @echo "Compiling $< to $@"
        @$(CC) -o $@ -c $< $(CFLAGS)

.PHONY: clean mrproper

clean:
        @rm -rf *.o

mrproper: clean
        @rm -rf $(EXEC)


*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <malloc.h>
#include <assert.h>
#include <time.h>

#include <unistd.h>

#include <lo/lo.h>
#include <libcwiimote/wiimote_api.h>

typedef enum { FALSE = 0, TRUE = 1} boolean_t;

lo_address osc_address;

void show_help() {
  fprintf(stderr, "Usage : wii2osc -s <osc server> -p <osc port> [-w wiimote_address]\n\n");
  fprintf(stderr, "\t-s <osc server> : OSC server IP address (mandatory, no default)\n");
  fprintf(stderr, "\t-p <osc port> : OSC server port (mandatory, no default)\n");
  fprintf(stderr, "\t-a <address> : OSC base address (default : '/')\n");
  fprintf(stderr, "\t-x : use accelerometer (default : no)\n");
  fprintf(stderr, "\t-g : enable debugging (default : no)\n");
  fprintf(stderr, "\t-w <wiimote address> : Wiimote bluetooth hardware address (default : autodetect)\n\n");
}

boolean_t send_state(wiimote_t *wii, const char * base_address)
{
  char *address;
  int result;

  /* Alloc space for base_address + "state" (or "accel") and '\0' */ 
  address = (char *) malloc (strlen(base_address) + 7);

  /* Add "state" to base address */
  strcpy(address, base_address);
  strcat(address, "state");
  
  /* Send state data */
  result = lo_send(osc_address, address, "iiiiiiiiiii",
                   wii->keys.left,
                   wii->keys.right,
                   wii->keys.up,
                   wii->keys.down,
                   wii->keys.plus,
                   wii->keys.minus,
                   wii->keys.a,
                   wii->keys.b,
                   wii->keys.one,
                   wii->keys.two,
                   wii->keys.home);

  if (result == -1) {
    fprintf(stderr, "OSC error %d: %s\n", 
      lo_address_errno(osc_address), 
      lo_address_errstr(osc_address));
  }

  if (wii->mode.acc) {
    /* If accelerometer is On, return data */

    /* Add "accel" to base address */
    strcpy(address,base_address);
    strcat(address,"accel");
    
    /* Send acceleration data */
    result = lo_send(osc_address, address, "iii",
                     wii->axis.x,
                     wii->axis.y,
                     wii->axis.z);
    
    if (result == -1) {
      fprintf(stderr, "OSC error %d: %s\n", 
              lo_address_errno(osc_address), 
              lo_address_errstr(osc_address));
    }
  }

  free(address);

  return (result == -1 ? FALSE : TRUE);
}

void associate(wiimote_t *wiimote, char * hwaddr, boolean_t accel) 
{

  fprintf (stderr, "\nInitializing Wiimote connection\n");
  fprintf (stderr, "===============================\n");
  fprintf (stderr, "Please, press keys 1 & 2...\n");

  while (!hwaddr) {
    /* Autodetect hardware */
    if (wiimote_discover(wiimote, 1) == 1) {
      hwaddr = (char *) malloc (strlen(wiimote->link.r_addr));
      strcpy(hwaddr, wiimote->link.r_addr);
    }
  }
  
  /* Connect to wiimote (selected, or autodetected) */
  /* Loop until connected, or until end of times */
  while (wiimote_connect(wiimote, hwaddr) < 0) {
    fprintf(stderr, "Unable to open wiimote: %s, retrying...\n", wiimote_get_error());
  }
  
  fprintf(stderr, "Wiimote %s now connected\n", hwaddr);


  /* Enable||Disable accelerometer */
  wiimote->mode.acc = (accel ? 1 : 0);
  /* No nunchuck for now */
  wiimote->mode.ext = 0;		
  /* Light all LEDs */
  wiimote->led.one = wiimote->led.two = wiimote->led.three = wiimote->led.four = 1;

  /* Send all to wiimote */
  wiimote_update(wiimote);
}

int main (int argc, char** argv)
{
  wiimote_t wiimote = WIIMOTE_INIT;

  struct timeval last, now, start;

  int ledbits=1;
  int count=0;

  char c;
  char *oWiimoteAddress = NULL;
  char *oBaseAddress = NULL;;
  char *oOscServer = NULL;;
  char *oOscPort = NULL;
  boolean_t oUseAccel = FALSE;
  boolean_t oDebug = FALSE;
  boolean_t do_show_help = FALSE;

  char *temp_address = NULL;

  /* Parse options */
  while ((c = getopt(argc, argv, "a:w:s:p:hgx")) != -1 ) {
    switch (c) {
    case 'x':
      oUseAccel = TRUE;
      break;
    case 'g':
      oDebug = TRUE;
      break;
    case 'a':
      temp_address = (char *)malloc(strlen(optarg)+1);
      strcpy(temp_address,optarg);
      break;
    case 's':
      oOscServer = (char *)malloc(strlen(optarg)+1);
      strcpy(oOscServer,optarg);
      break;
    case 'p':
      oOscPort = (char *)malloc(strlen(optarg)+1);
      strcpy(oOscPort,optarg);
      break;
    case 'w':
      oWiimoteAddress = (char *)malloc(strlen(optarg)+1);
      strcpy(oWiimoteAddress,optarg);
      break;
    case 'h':
      do_show_help = TRUE;
      break;
    case '?':
      if (isprint (optopt))
        fprintf (stderr, "Unknown option `-%c'.\n", optopt);
      else
        fprintf (stderr, "Unknown option character `\\x%x'.\n", optopt);
      do_show_help = TRUE;
    default:
      show_help();
      exit(EXIT_FAILURE);
    }
  }

  /* Seems that user is lost */
  if (do_show_help) {
    show_help();
    exit(EXIT_FAILURE);
  }

  if (!temp_address) {
    /* No address given, we just put 'root' */
    oBaseAddress = (char *)malloc(2);
    oBaseAddress[0]='/';
    oBaseAddress[1]='\0';
  } else if (temp_address[strlen(temp_address)-1] != '/') {
    /* Address given but without trailing slash, we have to add one */
    oBaseAddress = (char *)malloc(strlen(temp_address)+2);
    strcpy(oBaseAddress,temp_address);
    strcat(oBaseAddress,"/");
  } else {
    /* Address well formatted with a trailing slash */
    oBaseAddress = (char *)malloc(strlen(temp_address)+1);
    strcpy(oBaseAddress,temp_address);
  }

  /* Temp address not needed anymore */
  free(temp_address);

  if (!oOscServer || !oOscPort) {
    fprintf(stderr, "OSC Server and OSC port are mandatory !\n");
    show_help();
    exit(EXIT_FAILURE);
  }

  /* Display parameters un debug mode */
  if (oDebug) {
    fprintf(stderr,"OSC server : %s\nOSC port : %s\nbase address : %s\n", oOscServer, oOscPort, oBaseAddress);
    fprintf(stderr,"Wiimote address : %s\n", (oWiimoteAddress == NULL ? "autodetect" : oWiimoteAddress));
    fprintf(stderr,"Use 3-axis accelerometer : %s\n", (oUseAccel ? "yes" : "no"));
  }

  /* Create OSC address */
  osc_address = lo_address_new(oOscServer, oOscPort);

  /* Associate Wiimote */
  associate(&wiimote, oWiimoteAddress, oUseAccel);

  /* Keep track of time to play w/ LEDs */
  gettimeofday(&start);
  gettimeofday(&last);
  
  /* Forever */
  while (wiimote_is_open(&wiimote)) {
    count++;
    gettimeofday(&now);
    

    /* Playing with LEDs. This let the user know that something happens */
    if ( ((last.tv_sec-start.tv_sec)*1000000+last.tv_usec+200000) 
         < (((now.tv_sec-start.tv_sec)*1000000)+now.tv_usec)) {
      last = now;
      ledbits <<= 1;

      if (ledbits > 8) 
        ledbits = 1;
    }
    
    wiimote.led.bits = ledbits;

    /* Synchronize with wiimote : update LEDs states and get status */
    wiimote_update(&wiimote);

    /*    if (wiimote_update(&wiimote) == WIIMOTE_ERROR) {
      wiimote_disconnect(&wiimote);
      execv(argv[0], argv);
      }*/

    /* NOTE : the above doesn't work. libwiimote doesnt return error values (cf 
       wiimote_event.c, line 364) */

    /* Send state via OSC */
    send_state(&wiimote, oBaseAddress);

    /* Print debugging info if needed */
    if (oDebug) {
      fprintf(stderr,
              "count=%d,left=%d,right=%d,up=%d,down=%d,plus=%d,minus=%d,a=%d,b=%d,one=%d,two=%d,home=%d;",
              count,
              wiimote.keys.left,
              wiimote.keys.right,
              wiimote.keys.up,
              wiimote.keys.down,
              wiimote.keys.plus,
              wiimote.keys.minus,
              wiimote.keys.a,
              wiimote.keys.b,
              wiimote.keys.one,
              wiimote.keys.two,
              wiimote.keys.home);

      if (oUseAccel)
        fprintf(stderr,"x=%d,y=%d,z=%d", wiimote.axis.x, wiimote.axis.y, wiimote.axis.z);

      fprintf(stderr,"\n");
    }

    /* You can do stuff like this, but DANGER ... */
    /*
    if (wiimote.keys.b && wiimote.keys.home) {
      wiimote_disconnect(&wiimote);
      exit(1);
    }*/
  }

  free(oOscServer);
  free(oOscPort);
  free(oBaseAddress);
  lo_address_free(osc_address);
}
