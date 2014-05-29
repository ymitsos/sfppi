/*
* 	sfppi-generic.c:
*	to compile gcc -o sfppi-generic sfppi-generic.c -lwiringPi
*
*	sfppi is free software: you can redistribute it and/or modify
*	it under the terms of the GNU Lesser General Public License as
*	published by the Free Software Foundation, either version 3 of the
*	License, or (at your option) any later version.
*
*	sfppi-generic is distributed in the hope that it will be useful,
*	but WITHOUT ANY WARRANTY; without even the implied warranty of
*	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*	GNU Lesser General Public License for more details.
*
*	You should have received a copy of the GNU Lesser General Public
*	License along with sfppi-generic.
*	If not, see <http://www.gnu.org/licenses/>.
***********************************************************************
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <wiringPi.h>
#include <wiringPiI2C.h>
#include <errno.h>
#include <unistd.h>

#define VERSION 0.1

int mychecksum(unsigned char start_byte, unsigned char end_byte);
int dump(char *filename);
int read_eeprom(void);
int read_sfp(void);
int xio,write_checksum;
unsigned char A50[256]; //only interested in the first 128 bytes

int main (int argc, char *argv[])
{
	int opt;
	write_checksum = 0;
	while ((opt = getopt(argc, argv, "rcd:")) !=-1) {
		switch (opt) {
		case 'r':
			read_sfp();
			break;
		case 'c':
			write_checksum = 1;
			read_sfp();
			break;
		case 'd':
			dump(optarg);
			break;
		default: /* '?' */
			fprintf(stderr,
			"Usage: %s\n"
			"	-r read the sfp or sfp+\n"
			"	-c calculate checksums bytes\n"
			"	-d filename - dump the eprom to a file\n"
			,argv[0]);
			exit(EXIT_FAILURE);
		}
	}
	if (argc <=1) {
			fprintf(stderr,
			"Usage: %s\n"
			"	-r read the sfp or sfp+\n"
			"	-c calculate checksum bytes\n"
			"	-d filename - save the eprom to a file\n"
			,argv[0]);
			exit(EXIT_FAILURE);
	}
}
int read_sfp(void)
{
	unsigned char transceiver[8];//8 bytes - address 3 to 10
	unsigned char vendor[16+1]; //16 bytes - address 20 to 35
	unsigned char oui[3]; //3 bytes - address 37 to 39
	unsigned char partnumber[16+1]; //16 bytes - address 40 to 55
	unsigned char revision[4]; //4 bytes - address 56 - 59
	unsigned char serial[16+1]; //16 bytes - address 68 to 83
	unsigned char date[8+1];//8 bytes - address 84 to 91 
	unsigned char vendor_spec[16]; //16 bytes - address 99 to 114
	int cwdm_wave;
	static char *connector[16] = {
                "Unknown",
                "SC",
                "Fibre Channel Style 1 copper connector",
                "Fibre Channel Style 2 copper connector",
                "BNC/TNC",
                "Fibre Channel coaxial headers",
                "FiberJack",
                "LC",
                "MT-RJ",
                "MU",
                "SG",
                "Optical pigtail"};

	printf("sfppi-generic Version:%0.1f\n\n",VERSION);

	//Copy eeprom SFP details into A50
	if(!read_eeprom()); else exit(EXIT_FAILURE);

	//print the connector type
	printf("Connector Type = %s",connector[A50[2]]);

	//print the transceiver type
	if(A50[6] & 16) printf("\nTransceiver is 100Base FX");
        if(A50[6] & 8) printf("\nTransceiver is 1000Base TX");
        if(A50[6] & 4) printf("\nTransceiver is 1000Base CX");
        if(A50[6] & 2) printf("\nTransceiver is 1000Base LX");
        if(A50[6] & 1) printf("\nTransceiver is 1000Base SX");
        if(A50[6] & 0) printf("\nTransceiver is unknown");

        if(A50[3] & 64) printf("\nTransceiver is 10GBase-ER");
        if(A50[3] & 32) printf("\nTransceiver is 10GBase-LRM");
        if(A50[3] & 16) printf("\nTransceiver is 10GBase-LR");
        if(A50[4] & 12) printf("\nTransceiver is 10GBase-ZR");
        if(A50[3] & 8) printf("\nTransceiver is 10GBase-SR");

        //3 bytes.60 high order, 61 low order.62 is the
        //print vendor id bytes 20 to 35
        cwdm_wave = ((int) A50[60]<<8) | ((int) A50[61]);
        printf("\nWavelength = %d.%d",cwdm_wave,A50[62]);

	//print vendor id bytes 20 to 35
	memcpy(&vendor, &A50[20],16);
	vendor[16] = '\0';
	printf("\nVendor = %s",vendor);

	//Print partnumber values address 40 to 55  
	memcpy(&partnumber, &A50[40], 16);
	partnumber[16] = '\0';
	printf("\nPartnumber = %s", partnumber);

	//Print serial values address 68 to 83  
	memcpy(&serial, &A50[68], 16);
	serial[16] = '\0';
	printf("\nSerial = %s", serial);

	//Print date values address 84 to 91  
	memcpy(&date, &A50[84], 8);
	date[8] = '\0';
	printf("\ndate = %s", date);

	//Calculate the checksum: Add up the first 31 bytes and store
	//the last 8 bits in the 32nd byte
	mychecksum(0x0, 0x3f);

	//Calculate the extended checksum: Add up 31 bytes and store
	//the last 8 bits in the 32nd byte
	mychecksum(0x40, 0x5f);

  return 0;
}

int dump(char *filename)
{
	int j;
        unsigned char index_start = 0;
	unsigned char index_end = 0;
        int i=0;
        int counter = 0x0;
        FILE *fp;

	//Copy eeprom SFP details into A50
	if(!read_eeprom()); else exit(EXIT_FAILURE);

	fp=fopen(filename,"w");
        if (fp == NULL) {
                 fprintf (stderr, "File not created okay, errno = %d\n",
				 strerror (errno));
                return 1;
        }
        printf ("Dump eeprom contents to %s\n",filename) ;
        fprintf(fp,"     0   1   2   3   4   5   6   7   8   9   a   b  "
		       " c   d   e   f   0123456789abcdef");
        while (counter < 0x100){ //address 0 to 95
		if ((counter % 0x10) == 0){
                       index_end = counter;
                        fprintf(fp,"  ");
                        for(j = index_start; j <index_end; j++) {
                                if(A50[index_start] == 0x0 ||
                                        A50[index_start] == 0xff)
                                        fprintf(fp,".");
                                else
                                if (A50[index_start] < 32 ||
                                        A50[index_start] >=127){
                                        fprintf(fp,"?");
                                }
                                else
                                fprintf(fp,"%c",A50[index_start]);
                                index_start++;
                        }
                        index_start = index_end;
                        fprintf(fp,"\n%02x:",i);
                }
                fprintf(fp,"  %02x",A50[counter]);
                counter = counter + 1;
        }
        fclose(fp);
        return 0;
}

int mychecksum(unsigned char start_byte, unsigned char end_byte)
{
	int sum = 0;
	int counter;
	int cc_base;
	for (counter = start_byte; counter <end_byte; counter++)
	sum = (A50[counter] + sum);
	sum = sum & 0x0ff;
	cc_base = A50[end_byte]; //sum stored in address 63 or 95.
	if (start_byte == 0x0) printf("\ncc_base = %x, sum = %x",cc_base,sum);
	else printf("\nextended cc_base = %x and sum= %x\n",cc_base,sum);
		if (cc_base != sum && write_checksum){
			printf("\nCheck Sum failed, Do you want to write the"
			       "	checksum value %x to address byte \"%x\" ?"
			       "	(Y/N)", sum, end_byte);
			int ch = 0;
			ch = getchar();
			getchar();
			if ((ch == 'Y') || (ch == 'y')) {
				printf("yes\n");
				wiringPiI2CWriteReg8(xio, end_byte, sum);
			} else printf("nothing written\n");
		}
		return 0;
}

int read_eeprom(void)
{
	int xio,i,fd1;
        xio = wiringPiI2CSetup (0x50);
	if (xio < 0){
                fprintf (stderr, "xio: Unable to initialise I2C: %s\n",
                                strerror (errno));
                return 1;
        }
        /*Read in the first 128 bytes 0 to 127*/
        for(i=0; i <128; i++){
                fd1 = wiringPiI2CReadReg8 (xio,i);
                A50[i] = fd1;
		if (fd1 <0){
                fprintf (stderr, "xio: Unable to read address 0x50: %s\n",
                                strerror (errno));
			      return 1;
			}
	}
	return 0;
}
