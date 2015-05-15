/* Extenmote : NES, SNES, N64 and Gamecube to Wii remote adapter firmware
 * Copyright (C) 2012  Raphael Assenat <raph@raphnet.net>
 *
 * Based on:
 *
 *  GC to N64 : Gamecube controller to N64 adapter firmware
 *  Copyright (C) 2011  Raphael Assenat <raph@raphnet.net>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <string.h>
#include "gamepads.h"
#include "gamecube.h"
#include "gcn64_protocol.h"

/*********** prototypes *************/
static char gamecubeInit(void);
static char gamecubeUpdate(void);
static char gamecubeChanged(void);

/* What was most recently read from the controller */
static gamepad_data last_built_report;

/* What was most recently sent to the host */
static gamepad_data last_sent_report;

static char origins_set = 0;
static unsigned char orig_x, orig_y, orig_cx, orig_cy;

static char gamecubeInit(void)
{
	gamecubeUpdate();
	return 0;
}

void gc_decodeAnswer()
{
	char tmp;
	int i;
	unsigned char x,y,cx,cy;

	// Note: Checking seems a good idea, adds protection
	// against corruption (if the "constant" bits are invalid, 
	// maybe others are : Drop the packet). 
	//
	// However, I have seen bit 2 in a high state. To be as compatible
	// as possible, I decided NOT to look at these bits since instead
	// of being "constant" bits they might have a meaning I don't know.

	// Check the always 0 and always 1 bits
#if 0
	if (gcn64_workbuf[0] || gcn64_workbuf[1] || gcn64_workbuf[2])
		return 1;

	if (!gcn64_workbuf[8])
		return 1;
#endif
	
/*
	(Source: Nintendo Gamecube Controller Protocol
		updated 8th March 2004, by James.)

	Bit		Function
	0-2		Always 0    << RAPH: Not true, I see 001!
	3		Start
	4		Y
	5		X
	6		B
	7		A
	8		Always 1
	9		L
	10		R
	11		Z
	12-15	Up,Down,Right,Left
	16-23	Joy X
	24-31	Joy Y
	32-39	C Joystick X
	40-47	C Joystick Y
	48-55	Left Btn Val
	56-63	Right Btn Val
 */

	last_built_report.pad_type = PAD_TYPE_GAMECUBE;
	last_built_report.gc.buttons = 0;

	for (i=0; i<16; i++) {
		if (gcn64_workbuf[i]) {
			last_built_report.gc.buttons |= 1<<i;
		}
	}

	for (x=0,i=0; i<8; i++) // X axis
		x |= gcn64_workbuf[i+16] ? (0x80>>i) : 0;
	
	for (y=0,i=0; i<8; i++) // Y axis
		y |= gcn64_workbuf[i+24] ? (0x80>>i) : 0;
	
	for (cx=0,i=0; i<8; i++) // C X axis
		cx |= gcn64_workbuf[i+32] ? (0x80>>i) : 0;
	last_built_report.gc.cx = cx;

	for (cy=0,i=0; i<8; i++) // C Y axis
		cy |= gcn64_workbuf[i+40] ? (0x80>>i) : 0;	
	last_built_report.gc.cy = cy;


	if (origins_set) {
		last_built_report.gc.x = ((int)x-(int)orig_x);
		last_built_report.gc.y = ((int)y-(int)orig_y);
		last_built_report.gc.cx = ((int)cx-(int)orig_cx);
		last_built_report.gc.cy = ((int)cy-(int)orig_cy);
	} else {
		orig_x = x;
		orig_y = y;	
		orig_cx = cx;
		orig_cy = cy;	
		last_built_report.gc.x = 0;
		last_built_report.gc.y = 0;
		last_built_report.gc.cx = 0;
		last_built_report.gc.cy = 0;
		origins_set = 1;
	}


	for (tmp=0,i=0; i<8; i++) // Left btn value
		tmp |= gcn64_workbuf[i+48] ? (0x80>>i) : 0;
	last_built_report.gc.lt = tmp;
	
	for (tmp=0,i=0; i<8; i++) // Right btn value
		tmp |= gcn64_workbuf[i+56] ? (0x80>>i) : 0;	
	last_built_report.gc.rt = tmp;

	// Copy all the data as-is for the raw field
	memset(last_built_report.gc.raw_data, 0, GC_RAW_SIZE);
	for (i=0; i<64; i++) {
		if (gcn64_workbuf[i]) {
			last_built_report.gc.raw_data[i/8] |= 0x80 >> (i%8);
		}
	}
}


static char gamecubeUpdate()
{
	//unsigned char tmp=0;
	unsigned char tmpdata[8];
	unsigned char count;

#if 0
	/* The GetID command. This is required for the Nintendo Wavebird to work... */
	tmp = GC_GETID;
	count = gcn64_transaction(&tmp, 1);
	if (count != GC_GETID_REPLY_LENGTH) {
		return 1;
	}

	/* 
	 * The wavebird needs time. It does not answer the 
	 * folowwing get status command if we don't wait here.
	 *
	 * A good 2:1 safety margin has been chosen.
	 *
	 */
	// 10 : does not work
	// 20 : does not work
	// 25 : works
	// 30 : works
	_delay_us(50);
#endif

	tmpdata[0] = GC_GETSTATUS1;
	tmpdata[1] = GC_GETSTATUS2;
	tmpdata[2] = GC_GETSTATUS3(0);
	
	count = gcn64_transaction(tmpdata, 3);
	if (count != GC_GETSTATUS_REPLY_LENGTH) {
		return 1;
	}

	gc_decodeAnswer();

	return 0;
}


static char gamecubeProbe(void)
{
	origins_set = 0;

	if (gamecubeUpdate()) {
		return 0;
	}

	return 1;
}

static char gamecubeChanged(void)
{
	return memcmp(&last_built_report, &last_sent_report, sizeof(gamepad_data));
}

static void gamecubeGetReport(gamepad_data *dst)
{
	if (dst)
		memcpy(dst, &last_built_report, sizeof(gamepad_data));
}

Gamepad GamecubeGamepad = {
	.init					= gamecubeInit,
	.update					= gamecubeUpdate,
	.changed				= gamecubeChanged,
	.getReport				= gamecubeGetReport,
	.probe					= gamecubeProbe,
};

Gamepad *gamecubeGetGamepad(void)
{
	return &GamecubeGamepad;
}

