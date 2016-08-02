/* 
    Author: 	 Akash Gajjar <akash.gajjar@einfochips.com>
    Description: This code flash the msp430 on over bsl protocol, 
    		 code it self enter the msp430 into bsl mode,
		 flash the msp430, exit bsl mode.
 */

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/ioctl.h>
#include <string.h>
#include <termios.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <linux/i2c-dev.h>
#include <pthread.h>
#include <signal.h>
#include <dirent.h>
#include <errno.h>

#include "MSP430_FwUpgrade.h"

struct timeval g_x_Timeout;
INT32 g_i32_Version, g_i32_Revision;
UINT8 g_u8_UartDevFd = -1;
CHAR *g_pi8_CodePath = NULL;
fd_set g_fd_read, g_fd_write;

INT16 calchecksum(char *pbyte, int totalsize)
{
	INT16 checksum, xorsum = 0;
	INT32 i;
	checksum = 0;
		
  	for (i = 0; i < totalsize - 2; i = i + 2) {
	  xorsum ^= (pbyte[i] & 0xFF) + ((256 * pbyte[i+1]) & 0xFF00);
	}
	
	return (~xorsum);
}

INT16 Ascci_to_Hex(short int data)
{ 
	//	printf(" %d->",data);
	if(data<58 && data>47){
		//	printf("1->");
		return (data - 48);
	}else if(data < 71){
		//		printf("2->");
		return (data - 55);
	}	else{
		//	printf("3->");
		return (data - 87);
	}
}

INT32 init_Msp430_UART(int BaudRate)
{
	struct termios term_options;

	g_u8_UartDevFd = open(UART2_DEV, O_RDWR);
	if(g_u8_UartDevFd < 1)
	{
		perror("ttyO2");
		return FAIL;
	}
	
	/* Get control parameters of UART */
	tcgetattr(g_u8_UartDevFd , &term_options);

	/* clear previously set values */
	term_options.c_cflag &= ~CBAUD;
	term_options.c_cflag &= ~CBAUDEX;
	term_options.c_cflag &= ~CSIZE;
	
	/*set requested baud rate value.*/
	if(BaudRate == 9600)
	      term_options.c_cflag |= B9600;
	else if(BaudRate == 115200)
	      term_options.c_cflag |= B115200;
	
	/*set requested data bits.*/
	term_options.c_cflag |= CS8;

	/*set requested stop bits.*/
	term_options.c_cflag &= ~CSTOPB;

	/*set requested parity value.*/
	if(BaudRate == 9600) {
		term_options.c_cflag |= PARENB;
		term_options.c_cflag &= ~PARODD;
	} else {
		term_options.c_cflag &= ~PARENB;
	}

	term_options.c_lflag &= ~ECHO;
	term_options.c_lflag &= ~ISIG;
	term_options.c_lflag &= ~ICANON;

	//term_options.c_lflag = 0;
	term_options.c_oflag &= ~OPOST;

	/* Set control parameters of UART */
	tcsetattr(g_u8_UartDevFd, TCSANOW, &term_options);
		
	FD_ZERO(&g_fd_read);
	FD_SET(g_u8_UartDevFd, &g_fd_read);

	return SUCCESS;
}

INT32 SendAckByte()
{
	CHAR c = 0x80;
	CHAR readval = -1;
	
	g_x_Timeout.tv_sec = 1;
	g_x_Timeout.tv_usec = 0;
	
	tcflush(g_u8_UartDevFd, TCIOFLUSH);
	if (write(g_u8_UartDevFd, &c, sizeof (c)) < 0) {
		      fprintf(stderr, "Msp430 Firmware: write failed\n");
		      return FAIL;
	}
	
	memset(&readval, 0, 1);
	if (-1 == (select(g_u8_UartDevFd + 1, &g_fd_read, 
					      NULL, NULL, &g_x_Timeout))) {
		printf("Msp430 Firmware: select system call failed.\n");
		return FAIL;
	}
	
	if(FD_ISSET(g_u8_UartDevFd, &g_fd_read)) {
		if(read(g_u8_UartDevFd, &readval, 1) < 0) {
			printf("Msp430 Firmware: Error In Reading From UART2.\n");
			return FAIL;
		}
		
		if (readval != 0x90) {
			fprintf(stderr, "Msp430 Firmware: %s, failed to get ack 0x90\n",
									__func__);
			return FAIL;
		}
	} else {
		printf("Msp430 Firmware: %s, Failed To Get ACK From MSP430.\n",
									__func__);
		return FAIL;
	}
	return SUCCESS;
}

INT32 read_password()
{
	INT32 count, l = 0;;
	INT16 checksum;
 	CHAR cmd[] = {0x80,0x14,0x04,0x04,0xE0,0xFF,0x10,0x00,0x00,0x00};
	CHAR resp [64] = {0};
	CHAR cnt;
	CHAR CsumHB, CsumLB;

	checksum = calchecksum(cmd, sizeof(cmd)/sizeof(cmd[0]));
	CsumHB = (checksum & 0xFF00) >> 8;
	CsumLB = (checksum & 0x00FF);

 	cmd[8] = CsumLB;
 	cmd[9] = CsumHB;
		
	for (count = 0; count < sizeof (cmd)/sizeof (cmd[0]); count++) {
	    usleep(2000);
	    write(g_u8_UartDevFd, &cmd[count], 1);
	}
	
	g_x_Timeout.tv_sec = 2;
	g_x_Timeout.tv_usec = 0;

	if (-1 == (select(g_u8_UartDevFd + 1, &g_fd_read,
					      NULL, NULL, &g_x_Timeout)))
	{
		printf("Msp430 Firmware: select system call failed.\n");
		return FAIL;
	}
	
	if(FD_ISSET(g_u8_UartDevFd, &g_fd_read))
	{
		sleep(1);
		if((cnt=read(g_u8_UartDevFd, &resp, 64)) < 0) {
			printf("Msp430 Firmware: Error In Reading From version.\n");
			return FAIL;
		}
		
		for (count = 4; count < cnt; count++)
		{
			printf("%02x ", resp[count]);
			if (l++ == 15) {
			      printf("\n");
			      l = 0;
			}
		}
		printf("\n");
	} else {
	  	printf("Msp430 Firmware: %s Failed To Get ACK From MSP430.\n", 
								      __func__);
	}
	return SUCCESS;
}

INT32 read_sector()
{
	INT32 count, l = 0;
	INT16 checksum;
 	CHAR cmd[] = {0x80,0x14,0x04,0x04,0x00,0xC0,0x80,0x00,0x00,0x00};
	CHAR resp[0x84] = {0};
	CHAR cnt;
	CHAR CsumHB, CsumLB;

	checksum = calchecksum(cmd, sizeof(cmd)/sizeof(cmd[0]));
	CsumHB = (checksum & 0xFF00) >> 8;
	CsumLB = (checksum & 0x00FF);

 	cmd[8] = CsumLB;
 	cmd[9] = CsumHB;

	for (count = 0; count < sizeof (cmd)/sizeof (cmd[0]); count++) {
	    usleep(2000);
	    write(g_u8_UartDevFd, &cmd[count], 1);
	}
	
	g_x_Timeout.tv_sec = 2;
	g_x_Timeout.tv_usec = 0;

	if (-1 == (select(g_u8_UartDevFd + 1, &g_fd_read,
					      NULL, NULL, &g_x_Timeout)))
	{
		printf("Msp430 Firmware: select system call failed.\n");
		return FAIL;
	}
	
	if(FD_ISSET(g_u8_UartDevFd, &g_fd_read))
	{
	    sleep(1);
	    if((cnt=read(g_u8_UartDevFd, &resp, 0x84)) < 0) {
		    printf("Msp430 Firmware: Error In Reading From version.\n");
		    return -1;
	    }

	    for (count = 4; count < cnt; count++) {
			printf("%02x ", resp[count]);
			if (l++ == 15) {
			      printf("\n");
			      l = 0;
			}
	    }
	    printf("\n");

	} else {
		printf("Msp430 Firmware: %s Failed To Get ACK From MSP430.\n", 
								__func__);
	}
	return SUCCESS;
}

INT32 write_password()
{
	INT32 count;
	INT16 checksum;
 	CHAR cmd[] = {
			0x80,0x12,0x14,0x14,0xE0,0xFF,0x10,0x00,
			0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
			0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
			0x00,0x00
		      };
	CHAR resp [64] = {0};
	CHAR cnt, CsumHB, CsumLB;
	
	checksum = calchecksum(cmd, sizeof(cmd)/sizeof(cmd[0]));
	CsumHB = (checksum & 0xFF00) >> 8;
	CsumLB = (checksum & 0x00FF);

 	cmd[24] = CsumLB;
 	cmd[25] = CsumHB;
		
	for (count = 0; count < sizeof (cmd)/sizeof (cmd[0]); count++) {
	    usleep(2000);
	    write(g_u8_UartDevFd, &cmd[count], 1);
	}
	
	g_x_Timeout.tv_sec = 5;
	g_x_Timeout.tv_usec = 0;

	if (-1 == (select(g_u8_UartDevFd + 1, &g_fd_read, 
					    NULL, NULL, &g_x_Timeout))) {
		printf("Msp430 Firmware: select system call failed.\n");
		return FAIL;
	}
	
	if(FD_ISSET(g_u8_UartDevFd, &g_fd_read)) {
		sleep(1);
		if((cnt=read(g_u8_UartDevFd, &resp, 64)) < 0) {
			printf("Msp430 Firmware: Error In Reading From version.\n");
			return FAIL;
		}
	} else {
	  	printf("Msp430 Firmware: %s, Failed To Get ACK From MSP430.\n", 
									  __func__); 
		return FAIL;
	}
	return SUCCESS;
}

int set_passwd()
{
	INT32 count;
	INT16 checksum;
 	CHAR cmd[] = {
			    0x80,0x10,0x24,0x24,0x00,0x00,0x00,0x00, 
			    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
			    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
			    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
			    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
			    0x00,0x00,
		    };

	CHAR resp[20] = {0};
	CHAR cnt, CsumHB, CsumLB;

	checksum = calchecksum(cmd, sizeof(cmd)/sizeof(cmd[0]));
	CsumHB = (checksum & 0xFF00) >> 8;
	CsumLB = (checksum & 0x00FF);

 	cmd[40] = CsumLB;
 	cmd[41] = CsumHB;
		
	for (count = 0; count < sizeof (cmd)/sizeof (cmd[0]); count++) {
	    usleep(2000);
	    write(g_u8_UartDevFd, &cmd[count], 1);
	}
	
	g_x_Timeout.tv_sec = 5;
	g_x_Timeout.tv_usec = 0;

	if (-1 == (select(g_u8_UartDevFd + 1, &g_fd_read,
					    NULL, NULL, &g_x_Timeout))) {
		printf("Msp430 Firmware: select system call failed.\n");
		return FAIL;
	}
	
	if(FD_ISSET(g_u8_UartDevFd, &g_fd_read)) {
		if((cnt=read(g_u8_UartDevFd, &resp, 32)) < 0) {
			printf("Msp430 Firmware: Error In Reading From version.\n");
			return -1;
		}
		
	} else {
	  	printf("Msp430 Firmware: %s, Failed To Get ACK From MSP430.\n",
									__func__); 
		return FAIL;
	}
	return SUCCESS;
}

INT32 main_erase()
{
	INT32 count;
	INT16 checksum;
 	CHAR cmd[] = {0x80,0x18,0x04,0x04,0x00,0xc0,0x04,0xA5,0x00,0x00};	//mass erase 
	CHAR resp[16] = {0};
	CHAR cnt, CsumHB, CsumLB;
	
	checksum = calchecksum(cmd, sizeof(cmd)/sizeof(cmd[0]));
	CsumHB = (checksum & 0xFF00) >> 8;
	CsumLB = (checksum & 0x00FF);

 	cmd[8] = CsumLB;
 	cmd[9] = CsumHB;
		
	for (count = 0; count < sizeof (cmd)/sizeof (cmd[0]); count++) {
	    usleep(2000);
	    write(g_u8_UartDevFd, &cmd[count], 1);
	}
	
	g_x_Timeout.tv_sec = 2;
	g_x_Timeout.tv_usec = 0;

	if (-1 == (select(g_u8_UartDevFd + 1, &g_fd_read, 
					     NULL, NULL, &g_x_Timeout))) {
		printf("Msp430 Firmware: select system call failed.\n");
		return FAIL;
	}
	
	if(FD_ISSET(g_u8_UartDevFd, &g_fd_read))
	{
		if((cnt=read(g_u8_UartDevFd, &resp, 20)) < 0) {
			printf("Msp430 Firmware: Error In Reading From version.\n");
			return FAIL;
		}
		
		for (count = 0; count < cnt; count++)
		  printf("%02x ", resp[count]);
		printf("\n");
	} else {
	  	printf("Msp430 Firmware: %s, Failed To Get ACK From MSP430.\n", 
									__func__); 
		return FAIL;
	}
	return SUCCESS;
}

INT32 erase_check()
{
	INT32 count;
	INT16 checksum;
 	CHAR cmd[] = {0x80,0x1C,0x04,0x04,0x00,0xC0,0x00,0x30,0x00,0x00};	//erase check for 3k
	CHAR resp[16] = {0};
	CHAR cnt, CsumHB, CsumLB;
	
	checksum = calchecksum(cmd, sizeof(cmd)/sizeof(cmd[0]));
	CsumHB = (checksum & 0xFF00) >> 8;
	CsumLB = (checksum & 0x00FF);

 	cmd[8] = CsumLB;
 	cmd[9] = CsumHB;
		
	for (count = 0; count < sizeof (cmd)/sizeof (cmd[0]); count++) {
	    usleep(2000);
	    write(g_u8_UartDevFd, &cmd[count], 1);
	}
	
	g_x_Timeout.tv_sec = 2;
	g_x_Timeout.tv_usec = 0;

	if (-1 == (select(g_u8_UartDevFd + 1, &g_fd_read, 
					    NULL, NULL, &g_x_Timeout))) {
		printf("Msp430 Firmware: select system call failed.\n");
		return FAIL;
	}
	
	if(FD_ISSET(g_u8_UartDevFd, &g_fd_read))
	{
	    if((cnt=read(g_u8_UartDevFd, &resp, 20)) < 0) {
		    printf("Msp430 Firmware: Error In Reading From version.\n");
		    return FAIL;
	    }
	} else {
	  	printf("Msp430 Firmware: %s, Failed To Get ACK From MSP430.\n", 
									__func__); 
		return FAIL;
	}
	return SUCCESS;
}

int mass_erase()
{
	INT32 count;
	INT16 checksum;
 	CHAR cmd[] = {0x80,0x18,0x04,0x04,0x00,0x00,0x06,0xA5,0x00,0x00};	//mass erase 
	CHAR resp[16] = {0};
	CHAR cnt, CsumHB, CsumLB;
	
	checksum = calchecksum(cmd, sizeof(cmd)/sizeof(cmd[0]));
	CsumHB = (checksum & 0xFF00) >> 8;
	CsumLB = (checksum & 0x00FF);

 	cmd[8] = CsumLB;
 	cmd[9] = CsumHB;
		
	for (count = 0; count < sizeof (cmd)/sizeof (cmd[0]); count++) {
	    usleep(2000);
	    write(g_u8_UartDevFd, &cmd[count], 1);
	}
	
	g_x_Timeout.tv_sec = 10;
	g_x_Timeout.tv_usec = 0;

	if (-1 == (select(g_u8_UartDevFd + 1, &g_fd_read, 
					NULL, NULL, &g_x_Timeout))) {
		printf("Msp430 Firmware: select system call failed.\n");
		return FAIL;
	}
	
	if(FD_ISSET(g_u8_UartDevFd, &g_fd_read)) {
	    if((cnt=read(g_u8_UartDevFd, &resp, 32)) < 0) {
		    printf("Msp430 Firmware: Error In Reading From version.\n");
		    return FAIL;
	    }
	} else {
	  	printf("Msp430 Firmware: %s, Failed To Get ACK From MSP430.\n",
									__func__); 
		return FAIL;
	}
	return SUCCESS;
}

INT32 CheckBSLversion()
{
	INT32 count;
	INT16 checksum;
	CHAR resp[64] = {0};	
  	CHAR cmd[] = {0x80,0x1e,0x04,0x04,0x0b,0x0b,0x0b,0x0b,0x00,0x00};	//bsl version
	CHAR cnt=0, CsumHB, CsumLB;
	
	checksum = calchecksum(cmd, sizeof(cmd)/sizeof(cmd[0]));	
	CsumHB = (checksum & 0xFF00) >> 8;
	CsumLB = (checksum & 0x00FF);
 	
 	cmd[8] = CsumLB;
 	cmd[9] = CsumHB;

	for (count = 0; count < sizeof (cmd)/sizeof (cmd[0]); count++) {
		usleep(2000);
		write(g_u8_UartDevFd, &cmd[count], 1);
	}
	
	g_x_Timeout.tv_sec = 1;
	g_x_Timeout.tv_usec = 0;

	if (-1 == (select(g_u8_UartDevFd + 1, &g_fd_read, 
					    NULL, NULL, &g_x_Timeout))) {
		printf("Msp430 Firmware: select system call failed.\n");
		return FAIL;
	}
	
	if(FD_ISSET(g_u8_UartDevFd, &g_fd_read)) {
		sleep(1);
		if((cnt = read(g_u8_UartDevFd, &resp, 64)) < 0) {
			printf("Msp430 Firmware: Error In Reading From version.\n");
			return FAIL;
		}

	} else {
	  	printf("Msp430 Firmware: %s, Failed To Get ACK From MSP430.\n", 
									__func__); 
		return FAIL;
	}
	return SUCCESS;	
}

INT32 write_main()
{
	FILE *l_File_fp = NULL;
	CHAR buff[127];
	CHAR write_buff[30] = {0x80, 0x12};
	INT32 l_i32_l = 0;
	CHAR resp = 0, cnt;
	INT16 checksum, currentAddr = 0;
	UCHAR len,count, dataframelen = 0, linepos;
	
	l_File_fp = fopen(g_pi8_CodePath, "r");
	if (l_File_fp == NULL) {
	      fprintf(stderr, "Msp430 Firmware: source code file open error\n");
	      exit(2);
	}
	
	g_x_Timeout.tv_sec = 5;
	g_x_Timeout.tv_usec = 0;
	
	printf("Msp430 Firmware: Starting Firmware Upgrade .....\n");
	while(1){
		memset(&write_buff[6], 0, 20);
		fgets(buff, 127, l_File_fp);
		
		if(strstr(buff, "q")) {
			break;
		} else if(strstr(buff, "@")) {
			sscanf(&buff[1], "%hx\n", &currentAddr);
		} else {
			len = strlen(buff);
			for(linepos= 0; linepos < len-3; linepos+= 3, dataframelen++) {    
				sscanf(&buff[linepos], "%3x", &write_buff[8+dataframelen]);   
			}   
			  
			/* if frame is getting full => send frame */   
			if (dataframelen > 0) { 
				fprintf(stdout, "#");
				if (l_i32_l++ >= 120) {
					fprintf(stdout, "\n");
					l_i32_l = 0;
				}
				fflush(stdout);

				
				write_buff[2] = dataframelen + 4;
				write_buff[3] = dataframelen + 4;
				write_buff[4] = currentAddr & 0xff;
				write_buff[5] = (currentAddr >> 8 )& 0xff;
				write_buff[6] = dataframelen ;
				write_buff[7] = 0;

				checksum = calchecksum(write_buff,dataframelen+10);
				write_buff[dataframelen+8]= (checksum & 0xff);
				write_buff[dataframelen+9]= (checksum >> 8 )& 0xff;
				
				SendAckByte();
				for(count=0; count < dataframelen + 10; count++) {
					usleep(2000);
					write(g_u8_UartDevFd, &write_buff[count], 1);
				}
				
				//byteCtr+= dataframelen; /* Byte Counter */   
				currentAddr += dataframelen;    
				dataframelen = 0;
				
				if (-1 == (select(g_u8_UartDevFd + 1, &g_fd_read, 
								NULL, NULL, &g_x_Timeout))) {
					fprintf(stderr,"Msp430 Firmware: select system call failed.\n");
					return FAIL;
				}
				
				if(FD_ISSET(g_u8_UartDevFd, &g_fd_read)) {
					if((cnt = read(g_u8_UartDevFd, &resp, 1)) < 0) {
						fprintf(stderr, "Msp430 Firmware: Error In Reading From version.\n");
						return FAIL;
					}

					if (resp != 0x90) {
						fprintf(stderr, "Msp430 Firmware: %s, failed to get ack\n",
													  __func__);
						return FAIL;
					}
					  
				} else {
					fprintf(stderr, "Msp430 Firmware: %s, Failed To Get ACK From MSP430.\n", 
													  __func__);
					return FAIL;
				}
			}   
		}
	}
	printf("\nMsp430 Firmware: MSP430 Programmed\n");
	
	fclose(l_File_fp);
	return SUCCESS;
}

INT32 Detect_PTZ()
{
	UINT8 uartCmd[10] = {0};
	UINT8 resp[6] = {0};
	UINT8 count;
	UINT16 checksum = 0;
	
	/* UART_MSP_DETECT - UART MSP Detetct command */
	uartCmd[0] = DETECT_PTZ; 

	for(count = 0; count < 10 - 2 ; count++) {
	  checksum += uartCmd[count];
	}

	/* calculate checksum */
	uartCmd[8] = ((checksum & 0xFF00) >> 8 );
	uartCmd[9] = ((checksum & 0x00FF) );
	
	tcflush(g_u8_UartDevFd, TCIOFLUSH);
	if (write(g_u8_UartDevFd, uartCmd, sizeof(uartCmd)) < 0) {
		printf("Msp430 Firmware: CMD send failed\n");
		return FAIL;
	}
	
	g_x_Timeout.tv_sec = 2;
	g_x_Timeout.tv_usec = 0;
	
	if (-1 == (select(g_u8_UartDevFd + 1,
				  &g_fd_read, NULL, NULL, &g_x_Timeout))) {
		perror("Msp430 Firmware: select");
		return FAIL;
	}
		
	if(FD_ISSET(g_u8_UartDevFd, &g_fd_read)) {
		if (read(g_u8_UartDevFd, resp, 6) != 6) {
		      fprintf(stdout, "\nMsp430 Firmware: read failed From MSP430.\n");
		      return FAIL;
		} 
		fprintf(stdout, "\nMsp430 Firmware: Read value From UART2 = %d%c\n",
								      resp[0], resp[1]);
		if (resp[0] == 3 && resp[1] == 'A')
		      return SUCCESS;
	}
	
	return FAIL;
}

INT32 CheckDir(const char* DirPath)
{
	DIR *pDir;
	int bExists = FAIL;
	
	if (DirPath == NULL) 
	  return FAIL;

	pDir = opendir(DirPath);
	if (pDir != NULL) {
	    bExists = SUCCESS;    
	    (void) closedir(pDir);
	}

	return bExists;
}

INT32 ExportGpio(const char* GpioPin)
{
	FILE *ExpFp;
	size_t len;
	
	ExpFp = fopen(EXPORT_GPIO, "w");
	if (!ExpFp) {
	      perror("Msp430 Firmware: export open");
	      return FAIL;
	}
	
	len = strlen(GpioPin) + 1;
	fwrite(GpioPin, len, 1, ExpFp);

	fclose(ExpFp);
	return SUCCESS;
}

INT32 SetGpioDir(const char* GpioStatus, const char* Dir)
{
      	FILE *GpioFp;
	size_t len;
	
	GpioFp = fopen(GpioStatus, "w");
	if (!GpioFp) {
	      perror("Msp430 Firmware: GpioStatus open");
	      return FAIL;
	}
	
	len = strlen(Dir) + 1;
	fwrite(Dir, len, 1, GpioFp);
	
	fclose(GpioFp);
	return SUCCESS;	
}

INT32 Set_Gpio_Val(const char* GpioDir, const char Status)
{
	FILE *GpioFp;
	
	GpioFp = fopen(GpioDir, "w");
	if (!GpioFp) {
	      perror("Msp430 Firmware: GpioDir open");
	      return FAIL;
	}
	
	fwrite(&Status, sizeof(char), 1, GpioFp);
	
	fclose(GpioFp);
	return SUCCESS;
}

INT32 Bsl_Entry()
{
	INT32 DirExist;
	
	DirExist = CheckDir(SELECTION_GPIO_DIR);
	if (DirExist == FAIL) {
	      ExportGpio(SELECTION_GPIO_NO);
	      SetGpioDir(SELECTION_GPIO_STATUS, OUT_DIRECTION);
	}
	Set_Gpio_Val(SELECTION_GPIO_VALUE, SET_LOGIC_HIGH);
	
	DirExist = CheckDir(RESET_GPIO_DIR);
	if (DirExist == FAIL) {
	      ExportGpio(RESET_GPIO_NO);
	      SetGpioDir(RESET_GPIO_STATUS, OUT_DIRECTION);
	}
	Set_Gpio_Val(RESET_GPIO_VALUE, SET_LOGIC_LOW);
	
	DirExist = CheckDir(TEST_GPIO_DIR);
	if (DirExist == FAIL) {
	      ExportGpio(TEST_GPIO_NO);
	      SetGpioDir(TEST_GPIO_STATUS, OUT_DIRECTION);
	}
	Set_Gpio_Val(TEST_GPIO_VALUE, SET_LOGIC_LOW);
	
	usleep(10000);
	/* enter in bsl sequence */
	Set_Gpio_Val(TEST_GPIO_VALUE, SET_LOGIC_HIGH);
	usleep(10000);
	Set_Gpio_Val(TEST_GPIO_VALUE, SET_LOGIC_LOW);
	usleep(10000);
	Set_Gpio_Val(TEST_GPIO_VALUE, SET_LOGIC_HIGH);
	usleep(5000);
	Set_Gpio_Val(RESET_GPIO_VALUE, SET_LOGIC_HIGH);
	usleep(5000);
	Set_Gpio_Val(TEST_GPIO_VALUE, SET_LOGIC_LOW);
	
	return SUCCESS;
}

INT32 Bsl_Exit()
{
	Set_Gpio_Val(SELECTION_GPIO_VALUE, SET_LOGIC_LOW);
	usleep(1000);
	Set_Gpio_Val(RESET_GPIO_VALUE, SET_LOGIC_LOW);
	usleep(1000);
	Set_Gpio_Val(RESET_GPIO_VALUE, SET_LOGIC_HIGH);
	usleep(1000);
	Set_Gpio_Val(RESET_GPIO_VALUE, SET_LOGIC_LOW);
	usleep(1000);
	Set_Gpio_Val(RESET_GPIO_VALUE, SET_LOGIC_HIGH);
	
	return SUCCESS;	
}
INT32 Read_Code_VR() 
{
	FILE *l_FILE_Fp = NULL;
	INT8 buff[128];
	
	/* check source code is there */
	l_FILE_Fp = fopen(g_pi8_CodePath, "r");
	if (l_FILE_Fp == NULL){
	      fprintf(stderr, "Msp430 Firmware: ptz source code fopen failed\n");
	      return FAIL;
	}
	
	while(1) {
	    memset(buff, 0, sizeof(buff));
	    fgets(buff, sizeof(buff), l_FILE_Fp);
	    
	    if (strstr(buff, "q")) {
		fgets(buff, sizeof(buff), l_FILE_Fp);
		fprintf(stdout, "Msp430 Firmware: source code version revision = %s", buff);
		break;
	    }
	}
	
	sscanf(buff, "%d", &g_i32_Version);
	sscanf(buff + 2, "%d", &g_i32_Revision);
	
	fclose(l_FILE_Fp);
	return SUCCESS;
}

INT32 Detect_Ptz_Version()
{	
	UINT8 uartCmd[10] = {0};
	UINT8 resp[6] = {0};
	UINT8 count;
	UINT16 checksum = 0;

	Read_Code_VR();
	
	/* check ptz version revision */
	uartCmd[0] = READ_PTZ_VR;
	for(count = 0; count < 10 - 2 ; count++) {
		checksum += uartCmd[count];
	}

	/* calculate checksum */
	uartCmd[8] = ((checksum & 0xFF00) >> 8 );
	uartCmd[9] = ((checksum & 0x00FF) );

	/* call UART API */
	tcflush(g_u8_UartDevFd, TCIOFLUSH);
	if (write(g_u8_UartDevFd, uartCmd, sizeof(uartCmd)) < 0) {
		fprintf(stderr, "Msp430 Firmware: CMD send failed\n");
		return FAIL;
	}
	
	g_x_Timeout.tv_sec = 1;
	g_x_Timeout.tv_usec = 0;
	
	if (-1 == (select(g_u8_UartDevFd + 1, &g_fd_read, 
				      NULL, NULL, &g_x_Timeout))) {
		perror("Msp430 Firmware: select");
		return FAIL;
	}
		
	if(FD_ISSET(g_u8_UartDevFd, &g_fd_read)) {
		if (read(g_u8_UartDevFd, resp, 6) != 6) {
		      fprintf(stderr, "Msp430 Firmware: read failed From MSP430\n");
		      fprintf(stdout, "Msp430 Firmware: Firmware Need to be Upgrade\n");
		      
		      return NEED_TO_UPGRADE;
		} 
		fprintf(stdout, "Msp430 Firmware: Ptz Board Version Revision = %d.%d\n",
								resp[2], resp[3]);
	}
		
	if (g_i32_Version >= resp[2]) {
	      if (g_i32_Revision > resp[3]) {
		  fprintf(stdout, "Msp430 Firmware: Firmware Need to be Upgrade\n");
		  return NEED_TO_UPGRADE;
	      }
	}
	
	return NO_NEED_TO_UPGRADE;
}

INT32 Detect_Motor_Control_Board()
{
	INT32 l_i32_fd, l_i32_Res = FAIL;
	
	l_i32_fd = open("/dev/i2c-4", O_RDWR);
	if (l_i32_fd < 0) {
		perror("Msp430 Firmware: i2c-open");
		exit(1);
	}
	
	if (ioctl(l_i32_fd, I2C_SLAVE, 0x4C) < 0) {
		if (errno == EBUSY) {
			 printf("Msp430 Firmware: UU\n");
			 l_i32_Res = SUCCESS;
		} else {
			 printf("Msp430 Firmware: address doesent exist\n");
		}
	}
	
	close(l_i32_fd);
	return l_i32_Res;
}

INT32 main(int argc, char *argv[])
{
	INT32 l_i32_ret;
	INT32 l_i32_maxtries = 3;
	
	if (argc < 2) {
		fprintf(stderr, "Msp430 Firmware: Pass File Name To Flash\n");
		exit(1);
	}
	g_pi8_CodePath = argv[1];
	
	/* detect lm75 of motor control board */
	l_i32_ret = Detect_Motor_Control_Board();
	if (l_i32_ret == SUCCESS) {
	  
	  	init_Msp430_UART(115200);
		l_i32_ret = Detect_Ptz_Version();
		if (l_i32_ret == FAIL) {
			fprintf(stderr, "Msp430 Firmware: Detect Ptz failed\n");
			exit(1);
		} else if (l_i32_ret == NO_NEED_TO_UPGRADE) {
			fprintf(stderr, "Msp430 Firmware: Firmware is Already Newest\n");
			exit(2);
		}
		
		init_Msp430_UART(9600);
		Bsl_Entry();
	
		do {
			l_i32_ret = SendAckByte();
			if (l_i32_ret == SUCCESS) {
				set_passwd();
				l_i32_ret = write_main();
				if (l_i32_ret == SUCCESS) {
					break;
				} else {
					printf(" Trying Again %x\n", l_i32_maxtries);
					if (l_i32_maxtries == 0) {
						fprintf(stderr,"Msp430 Firmware:"
							  " msp430 flashing failed, exiting..\n");
						exit(EXIT_FAILURE);
					}
				}
			} else {
			      fprintf(stderr, "Msp430 Firmware:"
				     " Failed to get Ack from BSL Triel [ %d ]\n", l_i32_maxtries);
			}
		} while(l_i32_maxtries--);
		
		Bsl_Exit();
		sleep(1);
		
		init_Msp430_UART(115200);
		l_i32_ret = Detect_PTZ();
		if (l_i32_ret == SUCCESS) {
		      fprintf(stdout, "Msp430 Firmware: MSP430 Programming Test [ Pass ]\n");
		} else {
		      fprintf(stdout, "Msp430 Firmware: MSP430 Programming Test [ Fail ]\n");
		}
		close(g_u8_UartDevFd);
	} else {
		fprintf(stdout, "Msp430 Firmware: Motor control Board Detect Failed\n");
	}

	return SUCCESS;
}
