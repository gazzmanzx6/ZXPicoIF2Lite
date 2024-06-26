// Z80toROM - Z80 snapshot to ROM Cartrdige converter
// Copyright (c) 2023, Tom Dalby
// 
// Z80toROM is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// Z80toROM is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with Z80toROM. If not, see <http://www.gnu.org/licenses/>. 
//
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#define VERSION_NUM "v1.3"
#define PROGNAME "Z80toROM"

//v1.0 initial release
//v1.1 attempt to fix issue with earlier Spectrums
//v1.2 refactoring, bug fix on loader introduced in v1.1, handle pc in stack, stack in screen & ability to force final loader to screen
//v1.3 changed output header format and routine to create names from filename to match compressrom

// E00 - invalid option
// E01 - input file not Z80 or SNA snapshot
// E02 - cannot open snapshot for read
// E03 - cannot create output file 
// E04 - unsupported Z80 snapshot 
// E05 - special memory page mode, +3/+2A only so not supported
// E06 - not enough memory
// E07 - input file read error, issue with Z80/SNA snapshot
// E11 - cannot compress as won't fit into ROM
//
typedef union {
	uint32_t rrrr; //byte number
	uint8_t r[4]; //split number into 4 8bit bytes in case of overflow
} rrrr;
//
uint16_t dcz80(FILE** fp_in, uint8_t* out, uint16_t size);
uint32_t simplelz(uint8_t* fload, uint8_t* store, uint32_t filesize);
void error(uint8_t errorcode);
void printOut(FILE* fp, uint8_t* buffer, uint32_t filesize, char* name,uint8_t cm,char *oname);

//main
int main(int argc, char* argv[]) {
	if (argc < 2) {
		fprintf(stdout, "Usage: Z80toROM <-b> infile.z80/sna <displayname>\n");
		fprintf(stdout, "  -b also create binary files\n");
		fprintf(stdout, "  -s force final loader into screen\n");
		fprintf(stdout,"  if no displayname given infile filename will be used\n");		
		exit(0);
	}
	//
	uint8_t forceScreen=0,produceBinary = 0;
	uint8_t command=1;
	while(argv[command][0]=='-') {
		if (argv[command][1] == 'b') {
			produceBinary = 1;
		} else if(argv[command][1] == 's') {
			forceScreen = 1;
		} else {
			error(0);
		}
		command++;
	}
	// check infile is a snapshot
	if (strcmp(&argv[command][strlen(argv[command]) - 4], ".z80") != 0 && strcmp(&argv[command][strlen(argv[command]) - 4], ".Z80") != 0 &&
		strcmp(&argv[command][strlen(argv[command]) - 4], ".sna") != 0 && strcmp(&argv[command][strlen(argv[command]) - 4], ".SNA") != 0) error(1); // argument isn't .z80/sna or .Z80/SNA
	// create output file
	char* fZ80 = argv[command];
	char fROM[256]; // limit to 256chars
	int i=0;
	do {
		fROM[i] = fZ80[i];
		i++;
	} while(i<254&&(argv[command][i]!='.'||i<strlen(argv[command])-4));
	fROM[i] = '\0';
	// loader machine code
#define romReg_brd 34	// Border Colour
#define romReg_ffff 87	// restore 0xffff
#define romReg_fffd 121	// last OUT to 0xfffd
#define romReg_out 127	// last OUT to 0x7ffd
#define romReg_sp 147	// SP
#define romReg_jp 152	// jump into screen or stack
#define romReg_ay 154	// 16 AY Registers (39 - 54)
#define romReg_bca 170	// BC' 
#define romReg_dea 172	// DE' 
#define romReg_hla 174	// HL' 
#define romReg_ix 176	// IX
#define romReg_iy 178	// IY
#define romReg_afa 180	// AF' (F',A')
#define romReg_hl 182	// HL 
#define romReg_de 184	// DE 
#define romReg_bc 186	// BC 
#define romReg_f 188	// F
#define romReg_r 189	// R
#define romReg_bnks 229 // 128k banks if needed
#define romReg_len 236
	uint8_t romReg[] = { 0xf3,0x3e,0x80,0xed,0x47,0xaf,0xd3,0xfe,0x21,0x00,0x58,0x77,0x54,0x1e,0x01,0x01,
                             0xff,0x02,0xed,0xb0,0x21,0xbe,0x00,0x16,0x60,0x01,0x2e,0x00,0xed,0xb0,0xc3,0x00,
                             0x60,0x3e,0x00,0xd3,0xfe,0x31,0x00,0x00,0x21,0xec,0x00,0x11,0x00,0x40,0x43,0x18,
                             0x0d,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xed,0x4d,0x3c,0x4f,0xed,0xb0,0x7e,0x23,
                             0xfe,0x80,0x28,0x12,0x38,0xf4,0xd6,0x7e,0x4e,0x23,0xe5,0x62,0x6b,0xed,0x42,0x2b,
                             0x4f,0xed,0xb0,0xe1,0x18,0xe8,0x21,0x00,0x00,0xe5,0x31,0x9a,0x00,0x01,0xfd,0xff,
                             0xaf,0xe1,0xed,0x79,0x3c,0x06,0xbf,0xed,0x69,0x06,0xff,0xed,0x79,0x3c,0x06,0xbf,
                             0xed,0x61,0xfe,0x10,0x06,0xff,0x20,0xe9,0x3e,0x00,0xed,0x79,0x06,0x7f,0x3e,0x30,
                             0xed,0x79,0xd9,0xc1,0xd1,0xe1,0xd9,0xdd,0xe1,0xfd,0xe1,0x08,0xf1,0x08,0xe1,0xd1,
                             0xc1,0xf1,0x31,0x00,0x00,0xed,0x4f,0xc3,0xf5,0x57,0x00,0x00,0x00,0x00,0x00,0x00,
                             0x00,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0xbf,0x00,0x00,0xff,0x00,0xff,0x1a,0xf8,
                             0xf1,0xe3,0x3a,0x5c,0x8a,0x00,0x4c,0x10,0xcc,0x43,0x00,0x00,0x00,0x02,0x3a,0xff,
                             0x3f,0xd9,0x11,0x00,0x80,0x01,0x00,0x40,0x61,0x6c,0xed,0xb0,0xd9,0x21,0x27,0x60,
                             0x01,0xfd,0x7f,0x7e,0x23,0xb7,0xca,0x21,0x00,0xed,0x79,0xd9,0x44,0x65,0x16,0xc0,
                             0xed,0xb0,0xd9,0x18,0xee,0x30,0x00,0x13,0x14,0x16,0x17,0x00 };
 // 236bytes
// small code either in stack or screen
#define pcReg_im 6	// Interupt Mode
#define pcReg_a 8 	// A
#define pcReg_ei 9	// DI or EI
#define pcReg_jp 11	// PC
#define pcReg_len 13
	uint8_t pcReg[] = { 0x3a,0xff,0x3f,0xed,0x47,0xed,0x5e,0x3e,0x00,0xfb,0xc3,0xb7,0xd9 };
	uint8_t romReg_i = 0x00;
//
	//open read/write
	FILE* fp_in, * fp_out;
	if ((fp_in = fopen(fZ80, "rb")) == NULL) error(2); // cannot open snapshot for read
	// get filesize
	fseek(fp_in, 0, SEEK_END); // jump to the end of the file to get the length
	int filesize = ftell(fp_in); // get the file size
	rewind(fp_in);
	// z80 or sna?
	int snap = 0;
	if (strcmp(&fZ80[strlen(fZ80) - 4], ".sna") == 0 || strcmp(&fZ80[strlen(fZ80) - 4], ".SNA") == 0) snap = 1;
	//
	int otek = 0;
	uint8_t compressed = 0;
	rrrr addlen;
	addlen.rrrr = 0; // z80 only, 0 indicates v1, 23 for v2 otherwise v3
	uint8_t c;
	//read is sna, compressed=0, addlen.rrrr=0, otek=0
	if (snap) {
		if (filesize < 49179) error(7);
		if (filesize >= 131103) otek = 1; // 128k snapshot
		//	$00  I	Interrupt register
		romReg_i = fgetc(fp_in);
		//	$01  HL'
		romReg[romReg_hla] = fgetc(fp_in);
		romReg[romReg_hla + 1] = fgetc(fp_in);
		//	$03  DE'
		romReg[romReg_dea] = fgetc(fp_in);
		romReg[romReg_dea + 1] = fgetc(fp_in);
		// check this is a SNA snapshot
		if (romReg_i == 'M' && romReg[romReg_hla] == 'V' &&
			romReg[romReg_hla + 1] == ' ' && romReg[romReg_dea] == '-') error(7);
		if (romReg_i == 'Z' && romReg[romReg_hla] == 'X' &&
			romReg[romReg_hla + 1] == '8' && romReg[romReg_dea] == '2') error(7);		
		//	$05  BC'
		romReg[romReg_bca] = fgetc(fp_in);
		romReg[romReg_bca + 1] = fgetc(fp_in);
		//	$07  F'
		romReg[romReg_afa] = fgetc(fp_in);
		//	$08  A'
		romReg[romReg_afa + 1] = fgetc(fp_in);
		//	$09  HL	
		romReg[romReg_hl] = fgetc(fp_in);
		romReg[romReg_hl + 1] = fgetc(fp_in);
		//	$0B  DE
		romReg[romReg_de] = fgetc(fp_in);
		romReg[romReg_de + 1] = fgetc(fp_in);
		//	$0D  BC
		romReg[romReg_bc] = fgetc(fp_in);
		romReg[romReg_bc + 1] = fgetc(fp_in);
		//	$0F  IY
		romReg[romReg_iy] = fgetc(fp_in);
		romReg[romReg_iy + 1] = fgetc(fp_in);
		//	$11  IX
		romReg[romReg_ix] = fgetc(fp_in);
		romReg[romReg_ix + 1] = fgetc(fp_in);
		//	$13  0 for DI otherwise EI
		c = fgetc(fp_in);
		if (c == 0) pcReg[pcReg_ei] = 0xf3;	//di
		else pcReg[pcReg_ei] = 0xfb;	//ei
		//	$14  R
		romReg[romReg_r] = fgetc(fp_in);
		//	$15  F
		romReg[romReg_f] = fgetc(fp_in);
		//	$16  A
		pcReg[pcReg_a] = fgetc(fp_in);
		//	$17  SP
		romReg[romReg_sp] = fgetc(fp_in);
		romReg[romReg_sp + 1] = fgetc(fp_in);
		if (!otek) {
			if (romReg[romReg_sp] > 253) {
				romReg[romReg_sp + 1]++;
				romReg[romReg_sp] -= 254;
			}
			else {
				romReg[romReg_sp] += 2;
			}
		}
		// $19  Interrupt mode IM(0, 1 or 2)
		c = fgetc(fp_in) & 3;
		if (c == 0) pcReg[pcReg_im] = 0x46; //im 0
		else if (c == 1) pcReg[pcReg_im] = 0x56; //im 1
		else pcReg[pcReg_im] = 0x5e; //im 2
		//	$1A  Border colour
		c = fgetc(fp_in) & 7;
		romReg[romReg_brd] = c + 0x30;
	}
	// read z80
	else {
		//read in z80 starting with header
		//	0       1       A register
		pcReg[pcReg_a] = fgetc(fp_in);
		//	1       1       F register
		romReg[romReg_f] = fgetc(fp_in);
		//	2       2       BC register pair(LSB, i.e. C first)
		romReg[romReg_bc] = fgetc(fp_in);
		romReg[romReg_bc + 1] = fgetc(fp_in);
		//	4       2       HL register pair
		romReg[romReg_hl] = fgetc(fp_in);
		romReg[romReg_hl + 1] = fgetc(fp_in);
		//	6       2       Program counter (if zero then version 2 or 3 snapshot)
		pcReg[pcReg_jp] = fgetc(fp_in);
		pcReg[pcReg_jp + 1] = fgetc(fp_in);
		//	8       2       Stack pointer
		romReg[romReg_sp] = fgetc(fp_in);
		romReg[romReg_sp + 1] = fgetc(fp_in);
		//	10      1       Interrupt register
		romReg_i = fgetc(fp_in);
		//	11      1       Refresh register (Bit 7 is not significant!)
		c = fgetc(fp_in);
		romReg[romReg_r] = c;
		//	12      1       Bit 0: Bit 7 of r register; Bit 1-3: Border colour; Bit 4=1: SamROM; Bit 5=1:v1 Compressed; Bit 6-7: N/A
		c = fgetc(fp_in);
		compressed = (c & 32) >> 5;	// 1 compressed, 0 not
		if (c & 1 || c > 127) {
			romReg[romReg_r] = romReg[romReg_r] | 128;	// r high bit set
		}
		else {
			romReg[romReg_r] = romReg[romReg_r] & 127;	//r high bit reset
		}
		romReg[romReg_brd] = ((c & 14) >> 1) + 0x30; //border
		//	13      2       DE register pair
		romReg[romReg_de] = fgetc(fp_in);
		romReg[romReg_de + 1] = fgetc(fp_in);
		//	15      2       BC' register pair
		romReg[romReg_bca] = fgetc(fp_in);
		romReg[romReg_bca + 1] = fgetc(fp_in);
		//	17      2       DE' register pair
		romReg[romReg_dea] = fgetc(fp_in);
		romReg[romReg_dea + 1] = fgetc(fp_in);
		//	19      2       HL' register pair
		romReg[romReg_hla] = fgetc(fp_in);
		romReg[romReg_hla + 1] = fgetc(fp_in);
		//	21      1       A' register
		romReg[romReg_afa + 1] = fgetc(fp_in);
		//	22      1       F' register
		romReg[romReg_afa] = fgetc(fp_in);
		//	23      2       IY register (Again LSB first)
		romReg[romReg_iy] = fgetc(fp_in);
		romReg[romReg_iy + 1] = fgetc(fp_in);
		//	25      2       IX register
		romReg[romReg_ix] = fgetc(fp_in);
		romReg[romReg_ix + 1] = fgetc(fp_in);
		//	27      1       Interrupt flipflop, 0 = DI, otherwise EI
		c = fgetc(fp_in);
		if (c == 0) pcReg[pcReg_ei] = 0xf3;	//di
		else pcReg[pcReg_ei] = 0xfb;	//ei
		//	28      1       IFF2 [IGNORED]
		c = fgetc(fp_in);
		//	29      1       Bit 0-1: IM(0, 1 or 2); Bit 2-7: N/A
		c = fgetc(fp_in) & 3;
		if (c == 0) pcReg[pcReg_im] = 0x46; //im 0
		else if (c == 1) pcReg[pcReg_im] = 0x56; //im 1
		else pcReg[pcReg_im] = 0x5e; //im 2
		// version 2 & 3 only
		if (pcReg[pcReg_jp] == 0 && pcReg[pcReg_jp + 1] == 0) {
			//  30      2       Length of additional header block
			addlen.r[0] = fgetc(fp_in);
			addlen.r[1] = fgetc(fp_in);
			//  32      2       Program counter
			pcReg[pcReg_jp] = fgetc(fp_in);
			pcReg[pcReg_jp + 1] = fgetc(fp_in);
			//	34      1       Hardware mode standard 0-6 (2 is SamRAM), 7 +3, 8 +3 & 10 not supported, 11 Didatik, 12 +2, 13 +2A
			c = fgetc(fp_in);
			if (c == 2 || c == 10 || c == 11 || c > 13) error(4);
			if (addlen.rrrr == 23 && c > 2) otek = 1; // v2 & c>2 then 128k, if v3 then c>3 is 128k
			else if (c > 3) otek = 1;
			//	35      1       If in 128 mode, contains last OUT to 0x7ffd
			c = fgetc(fp_in);
			if (otek) romReg[romReg_out] = c;
			//	36      1       Contains 0xff if Interface I rom paged [SKIPPED]
			//	37      1       Hardware Modify Byte [SKIPPED]
			fseek(fp_in, 2, SEEK_CUR);
			//	38      1       Last OUT to port 0xfffd (soundchip register number)
			//	39      16      Contents of the sound chip registers
			romReg[romReg_fffd] = fgetc(fp_in);	// last out to $fffd (38)
			for (i = 0; i < 16; i++) romReg[romReg_ay + i] = fgetc(fp_in); // ay registers (39-54) 
			// following is only in v3 snapshots
			//	55      2       Low T state counter [SKIPPED]
			//	57      1       Hi T state counter [SKIPPED]
			//	58      1       Flag byte used by Spectator (QL spec.emulator) [SKIPPED]
			//	59      1       0xff if MGT Rom paged [SKIPPED]
			//	60      1       0xff if Multiface Rom paged. Should always be 0. [SKIPPED]
			//	61      1       0xff if 0 - 8191 is ROM, 0 if RAM [SKIPPED]
			//	62      1       0xff if 8192 - 16383 is ROM, 0 if RAM [SKIPPED]
			//	63      10      5 x keyboard mappings for user defined joystick [SKIPPED]
			//	73      10      5 x ASCII word : keys corresponding to mappings above [SKIPPED]
			//	83      1       MGT type : 0 = Disciple + Epson, 1 = Disciple + HP, 16 = Plus D [SKIPPED]
			//	84      1       Disciple inhibit button status : 0 = out, 0ff = in [SKIPPED]
			//	85      1       Disciple inhibit flag : 0 = rom pageable, 0ff = not [SKIPPED]
			if (addlen.rrrr > 23) fseek(fp_in, 31, SEEK_CUR);
			// only if version 3 & 55 additional length
			//	86      1       Last OUT to port 0x1ffd, ignored as only applicable on +3/+2A machines [SKIPPED]
			if (addlen.rrrr == 55) 	if ((fgetc(fp_in) & 1) == 1) error(5); //special page mode so exit as not compatible with earlier 128k machines
		}
	}
	// space for decompression of z80
	// 8 * 16384 = 131072bytes
	//     0-49152 - Pages 5,2 & 0 (main memory)
	// *128k only - 49152-65536: Page 1; 65536-81920: Page 3; 81920-98304: Page 4; 98304-114688: Page 6; 114688-131072: Page 7
	int fullsize = 49152;
	if (otek) {
		fullsize = 131072;
	}
	//
	uint8_t* main;
	if ((main = (uint8_t*)malloc(fullsize * sizeof(uint8_t))) == NULL) error(6); // cannot create space for decompressed z80 
	//
	rrrr len;
	len.rrrr = 0;
	//set-up bank locations
	int bank[11], bankend;
	for (i = 0; i < 11; i++) bank[i] = 99; //default
	if (otek) {
		bank[3] = 32768; //page 0
		bank[4] = 49152; //page 1
		bank[5] = 16384; //page 2
		bank[6] = 65536; //page 3
		bank[7] = 81920; //page 4
		bank[8] = 0; //page 5
		bank[9] = 98304; //page 6
		bank[10] = 114688; //page 7
		bankend = 8;
	}
	else {
		bank[4] = 16384; //page 2
		bank[5] = 32768; //page 0
		bank[8] = 0; //page 5
		bankend = 3;
	}
	if (addlen.rrrr == 0) { // SNA or version 1 z80 snapshot & 48k only
		if (!compressed) {
			if (fread(main, sizeof(uint8_t), 49152, fp_in) != 49152) error(7);
			// sort out pc for SNA
			if (snap) {
				uint32_t stackpos = romReg[romReg_sp] + romReg[romReg_sp + 1] * 256;
				if (stackpos == 0) stackpos = 65536;
				pcReg[pcReg_jp] = main[stackpos - 16384 - 2];
				pcReg[pcReg_jp + 1] = main[stackpos - 16384 - 1];
			}
		}
		else {
			if (dcz80(&fp_in, main, 49152) != 49152) error(7);
		}
		if (otek) {
			// PC
			pcReg[pcReg_jp] = fgetc(fp_in);
			pcReg[pcReg_jp + 1] = fgetc(fp_in);
			// last out to 0x7ffd
			romReg[romReg_out] = fgetc(fp_in);
			// TD-DOS
			if (fgetc(fp_in) != 0) error(7);
			uint32_t pagelayout[7];
			for (i = 0; i < 7; i++) pagelayout[i] = 99;
			pagelayout[0] = romReg[romReg_out] & 7;
			//
			if (pagelayout[0] == 0) {
				pagelayout[0] = 32768;
				pagelayout[1] = 49152;
				pagelayout[2] = 65536;
				pagelayout[3] = 81920;
				pagelayout[4] = 98304;
				pagelayout[5] = 114688;
			}
			else if (pagelayout[0] == 1) {
				pagelayout[0] = 49152;
				pagelayout[1] = 32768;
				pagelayout[2] = 65536;
				pagelayout[3] = 81920;
				pagelayout[4] = 98304;
				pagelayout[5] = 114688;
			}
			else if (pagelayout[0] == 2) {
				pagelayout[0] = 16384;
				pagelayout[1] = 32768;
				pagelayout[2] = 49152;
				pagelayout[3] = 65536;
				pagelayout[4] = 81920;
				pagelayout[5] = 98304;
				pagelayout[6] = 114688;
			}
			else if (pagelayout[0] == 3) {
				pagelayout[0] = 65536;
				pagelayout[1] = 32768;
				pagelayout[2] = 49152;
				pagelayout[3] = 81920;
				pagelayout[4] = 98304;
				pagelayout[5] = 114688;
			}
			else if (pagelayout[0] == 4) {
				pagelayout[0] = 81920;
				pagelayout[1] = 32768;
				pagelayout[2] = 49152;
				pagelayout[3] = 65536;
				pagelayout[4] = 98304;
				pagelayout[5] = 114688;
			}
			else if (pagelayout[0] == 5) {
				pagelayout[0] = 0;
				pagelayout[1] = 32768;
				pagelayout[2] = 49152;
				pagelayout[3] = 65536;
				pagelayout[4] = 81920;
				pagelayout[5] = 98304;
				pagelayout[6] = 114688;
			}
			else if (pagelayout[0] == 6) {
				pagelayout[0] = 98304;
				pagelayout[1] = 32768;
				pagelayout[2] = 49152;
				pagelayout[3] = 65536;
				pagelayout[4] = 81920;
				pagelayout[5] = 114688;
			}
			else {
				pagelayout[0] = 114688;
				pagelayout[1] = 32768;
				pagelayout[2] = 49152;
				pagelayout[3] = 65536;
				pagelayout[4] = 81920;
				pagelayout[5] = 98304;
			}
			if (pagelayout[0] != 32768) {
				for (i = 0; i < 16384; i++) main[pagelayout[0] + i] = main[32768 + i]; //copy 0->?
			}
			for (i = 1; i < 7; i++) {
				if (pagelayout[i] != 99) {
					if (fread(&main[pagelayout[i]], sizeof(uint8_t), 16384, fp_in) != 16384) error(7);
				}
			}
		}
	}
	// version 2 & 3
	else {
		//		Byte    Length  Description
		//		-------------------------- -
		//		0       2       Length of compressed data(without this 3 - byte header)
		//						If length = 0xffff, data is 16384 bytes longand not compressed
		//		2       1       Page number of block
		// for 48k snapshots the order is:
		//		0 48k ROM, 1, IF1/PLUSD/DISCIPLE ROM, 4 page 2, 5 page 0, 8 page 5, 11 MF ROM 
		//		only 4, 5 & 8 are valid for this usage, all others are just ignored
		// for 128k snapshots the order is:
		//		0 ROM, 1 ROM, 3 Page 0....10 page 7, 11 MF ROM.
		// all pages are saved and there is no end marker
		do {
			len.r[0] = fgetc(fp_in);
			len.r[1] = fgetc(fp_in);
			c = fgetc(fp_in);
			if (bank[c] != 99) {
				if (len.rrrr == 65535) {
					if (fread(&main[bank[c]], sizeof(uint8_t), 16384, fp_in) != 16384) error(7);
				}
				else {
					if (dcz80(&fp_in, &main[bank[c]], 16384) != 16384) error(7);
				}
			}
			bankend--;
		} while (bankend);
	}
	fclose(fp_in);
	//
	//              12345678901234567890123456789012345678901234567890123456789012345678901234567890
	//						 1         2         3         4         5         6         7         8
	fprintf(stdout,"  /----------------------------------------------------------------------------\\\n");
	fprintf(stdout," /|af$%02x%02x af'$%02x%02x hl$%02x%02x hl'$%02x%02x bc$%02x%02x bc'$%02x%02x de$%02x%02x de'$%02x%02x ix$%02x%02x |\n",
		pcReg[pcReg_a],romReg[romReg_f],romReg[romReg_afa+1],romReg[romReg_afa],
		romReg[romReg_hl+1],romReg[romReg_hl],romReg[romReg_hla+1],romReg[romReg_hla],
		romReg[romReg_bc+1],romReg[romReg_bc],romReg[romReg_bca+1],romReg[romReg_bca],
		romReg[romReg_de+1],romReg[romReg_de],romReg[romReg_dea+1],romReg[romReg_dea],
		romReg[romReg_ix+1],romReg[romReg_ix]);
	fprintf(stdout,"/ |iy$%02x%02x brd$%02x ",romReg[romReg_iy+1],romReg[romReg_iy],romReg[romReg_brd]);
	if (pcReg[pcReg_ei] == 0xf3) fprintf(stdout,"di ");	//di
	else fprintf(stdout,"ei ");	//ei
	if (pcReg[pcReg_im] == 0x46) fprintf(stdout,"im0 "); //im 0
	else if (pcReg[pcReg_im] == 0x56) fprintf(stdout,"im1 "); //im 1
	else fprintf(stdout,"im2 "); // im 2
	fprintf(stdout,"ir$%02x%02x pc$%02x%02x sp$%02x%02x ",romReg_i,romReg[romReg_r],pcReg[pcReg_jp + 1],pcReg[pcReg_jp],romReg[romReg_sp+1],romReg[romReg_sp]);
	if(romReg[romReg_r]<9) romReg[romReg_r]+=119;
	else romReg[romReg_r] -= 9; // so it is correct on launch
	// where to put the final loader?
	uint16_t stackPos = (romReg[romReg_sp + 1] * 256) + romReg[romReg_sp];
	uint16_t pcPos = (pcReg[pcReg_jp + 1] * 256) + pcReg[pcReg_jp];
	//fprintf(stdout,"%d %d gap:%d\n",stackPos,pcPos,stackPos-pcPos);
	if ((stackPos>pcPos&&(stackPos-pcPos<32))||forceScreen==1) {
		if(stackPos<16384&&stackPos>=(16384-pcReg_len)) {
			fprintf(stdout, "(Final Loader in Screen @%04x)|\n", 0x4000);
			for (i = 0; i < pcReg_len; i++) {
				main[i] = pcReg[i];
			}
			romReg[romReg_jp + 1] = 0x40;
			romReg[romReg_jp] = 0x00;			
		}
		else {
			fprintf(stdout, "(Final Loader in Screen @%04x)|\n", 0x5800-pcReg_len);
			for (i = 0; i < pcReg_len; i++) {
				main[(6144-pcReg_len) + i] = pcReg[i];
			}
			romReg[romReg_jp + 1] = 0x57;
			romReg[romReg_jp] = 0x100-pcReg_len;
		}
	}
	else {
		stackPos -= pcReg_len;
		fprintf(stdout, "(Final Loader in Stack @%04x) |\n", stackPos);
		for (i = 0; i < pcReg_len; i++) {
			main[stackPos - 16384 + i] = pcReg[i];
		}
		romReg[romReg_jp + 1] = stackPos / 256;
		romReg[romReg_jp] = stackPos - (romReg[romReg_jp] * 256);
	}
	if(otek) {
		fprintf(stdout,"  |128k -> 7ffd$%02x fffd$%02x ay",romReg[romReg_out],romReg[romReg_fffd]);
		for(i=0;i<16;i++) fprintf(stdout,"$%02x",romReg[romReg_ay+i]);
		fprintf(stdout,"  |\n");
	}
	fprintf(stdout,"  |----------------------------------------------------------------------------|\n");
	//              12345678901234567890123456789012345678901234567890123456789012345678901234567890
	//						 1         2         3         4         5         6         7         8
	//
	// compress every page and write out with
	uint8_t* store;
	uint32_t size = 49152;
	int banks = 3;
	if (otek) {
		banks = 8;
		size = 131072;
		romReg[romReg_bnks] = 0x10;
		romReg[romReg_bnks + 1] = 0x11;
	}
	romReg[romReg_ffff] = main[size - 2];
	romReg[romReg_ffff + 1] = main[size - 1];
	//
	rrrr cmsize;
	if ((store = (uint8_t*)malloc(size * sizeof(uint8_t))) == NULL) error(6);
	for (i = 0; i < size; i++) store[i] = 0x00; // clear store
	for (i = 0; i < romReg_len; i++) store[i] = romReg[i]; // copy in the loader
	cmsize.rrrr = simplelz(main, &store[romReg_len], 16384);
	fprintf(stdout, "  |ROM 0   (16384- 32767) Compressing Bank 5 (%5dbytes) + Loader (%3dbytes)  |\n", cmsize.rrrr, romReg_len);
	if (cmsize.rrrr >= (16384 - (romReg_len+1))) error(11);
	store[0x3fff]=romReg_i; // put i at end of ROM
	//
	fprintf(stdout, "  |ROM 1,2 (32768- 65535) Copying Banks 2 & 0                                  |\n");
	if (otek) fprintf(stdout, "  |ROM 3-7 (65536-131071) Copying Banks 1,3,4,6 & 7                            |\n");
	fprintf(stdout,"\\ |----------------------------------------------------------------------------|\n");
	for (i = 1; i < banks; i++) {
		for (int k = 0; k < 16384; k++) store[16384 * i + k] = main[16384 * i + k];
	}
	free(main);
	// if required produce the binary files, 16384bytes each
	strcat(fROM, ".0");
	if (produceBinary == 1) {
		for (i = 0; i < banks; i++) {
			if ((fp_out = fopen(fROM, "wb")) == NULL) error(3); // cannot open file for write	
			if (fwrite(&store[i * 16384], sizeof(uint8_t), 16384, fp_out) != 16384) error(7);
			fclose(fp_out);
			fROM[strlen(fROM) - 1]++;
		}
	}
	// compress the ROM ready for use on the interface
	uint8_t* comp;
	if ((comp = (uint8_t*)malloc(size * sizeof(uint8_t))) == NULL) error(6);
	cmsize.rrrr = simplelz(store, comp, size);
	fprintf(stdout, " \\|Full ROM Compressed to %5dbytes (%4.1f%% saving)                            |\n", cmsize.rrrr,(((double)fullsize-(double)cmsize.rrrr)/(double)fullsize)*100);
	fprintf(stdout,"  \\----------------------------------------------------------------------------/\n");
	free(store);
	// create ROM name
	char outName[33],headerName[33];

	//char headerName[256];
	//char outName[33];
	i=0;
	unsigned int j=0;
	if((fZ80[j]>='0'&&fZ80[j]<='9')) {
		headerName[j]='_';	// starts with a number
		j++;
	}
	do {
		outName[i]=fZ80[i];
		if(j<32) {
			if(fZ80[i]>='A'&&fZ80[i]<='Z') {
				headerName[j++]=fZ80[i]+32;
			} else if((fZ80[i]>='0'&&fZ80[i]<='9')||
					(fZ80[i]>='a'&&fZ80[i]<='z')) {
				headerName[j++]=fZ80[i];
			} else {
				headerName[j++]='_';
			}
		}
		i++;
	} while(i<32&&(fZ80[i]!='.'||i<strlen(fZ80)-4));
	outName[i]=headerName[j]='\0';
	fROM[strlen(fROM)-1] = 'h';
	if ((fp_out = fopen(fROM, "wb")) == NULL) error(3); // cannot open rom for write	
	//
	fprintf(fp_out,"// ,%s",headerName);
	for(i=strlen(headerName);i<35;i++) fprintf(fp_out," ");
	fprintf(fp_out,"// xx - %dbytes\n",cmsize.rrrr+34);
	if(command<argc-1) {
		printOut(fp_out, comp, cmsize.rrrr, headerName,otek,argv[argc-1]);
	} else {
		printOut(fp_out, comp, cmsize.rrrr, headerName,otek,outName);
	}	
	//
	fclose(fp_out);
	free(comp);
	// all done
	return 0;
}

//
// ---------------------------------------------------------------------------
// printOut - print out the binary in a standard header format
// ---------------------------------------------------------------------------
void printOut(FILE* fp, uint8_t* buffer, uint32_t filesize, char* name,uint8_t cm,char *oname) {
	uint32_t i, j;
	fprintf(fp, "    const uint8_t %s[]={ ", name);
	//
	if(cm) fprintf(fp,"0x08,");	// 128k
	else fprintf(fp,"0x03,");	// 48k
	fprintf(fp,"0x00,");	// spare
	j=0;
	do {
		if(j<strlen(oname)) fprintf(fp,"0x%02x,",oname[j]); 
		else fprintf(fp,"0x00,"); 
	} while(++j<32);
	fprintf(fp,"\n");
	for(j=0;j<23+strlen(name);j++) fprintf(fp," ");   
	//
	for (i = 0; i < filesize; i++) {
		if ((i % 32) == 0 && i != 0) {
			fprintf(fp, "\n");
			for (j = 0; j < 23 + strlen(name); j++) fprintf(fp, " ");
		}
		fprintf(fp, "0x%02x", buffer[i]);
		if (i < filesize - 1) {
			fprintf(fp, ",");
		}
	}
	fprintf(fp, " };\n");
}

//
// ---------------------------------------------------------------------------
// dcz80 - decompress z80 snapshot routine
// ---------------------------------------------------------------------------
uint16_t dcz80(FILE** fp_in, uint8_t* out, uint16_t size) {
	uint16_t i = 0, k, j;
	uint8_t c;
	while (i < size) {
		c = fgetc(*fp_in);
		if (c == 0xed) { // is it 0xed [0]
			c = fgetc(*fp_in); // get next
			if (c == 0xed) { // is 2nd 0xed then a sequence
				j = fgetc(*fp_in); // counter into j
				c = fgetc(*fp_in);
				for (k = 0; k < j; k++) out[i++] = c;
			}
			else {
				out[i++] = 0xed;
				fseek(*fp_in, -1, SEEK_CUR); // back one
			}
		}
		else {
			out[i++] = c; // just copy
		}
	}
	return i;
}

//
// ---------------------------------------------------------------------------
// simplelz - very simple lz with 256byte backward look
//   x=128+ then copy sequence from x-offset from next byte offset 
//   x=0-127 then copy literal x+1 times
//   minimum sequence size 2
// ---------------------------------------------------------------------------
uint32_t simplelz(uint8_t* fload, uint8_t* store, uint32_t filesize) {
	int i;
	uint8_t* store_p, * store_c;

	int litsize = 1;
	int repsize, offset, repmax, offmax;
	store_c = store;
	store_p = store_c + 1;
	//
	i = 0;
	*store_p++ = fload[i++];
	do {
		// scan for sequence
		repmax = 2;
		if (i > 255) offset = i - 256; else offset = 0;
		do {
			repsize = 0;
			while (fload[offset + repsize] == fload[i + repsize] && i + repsize < filesize && repsize < 129) {
				repsize++;
			}
			if (repsize > repmax) {
				repmax = repsize;
				offmax = i - offset;
			}
			offset++;
		} while (offset < i && repmax < 129);
		if (repmax > 2) {
			if (litsize > 0) {
				*store_c = litsize - 1;
				store_c = store_p++;
				litsize = 0;
			}
			*store_p++ = offmax - 1; //1-256 -> 0-255
			*store_c = repmax + 126;
			store_c = store_p++;
			i += repmax;
		}
		else {
			litsize++;
			*store_p++ = fload[i++];
			if (litsize > 127) {
				*store_c = litsize - 1;
				store_c = store_p++;
				litsize = 0;
			}
		}
	} while (i < filesize);
	if (litsize > 0) {
		*store_c = litsize - 1;
		store_c = store_p++;
	}
	*store_c = 128;	// end marker
	return store_p - store;
}

void error(uint8_t errorcode) {
	fprintf(stdout, "[E%02d]\n", errorcode);
	exit(errorcode);
}
