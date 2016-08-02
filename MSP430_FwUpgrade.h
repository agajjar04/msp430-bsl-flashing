#ifndef __MSP430_FW_UPGRADE__
#define __MSP430_FW_UPGRADE__

/* typedef */
typedef unsigned char UINT8;
typedef unsigned char UCHAR;
typedef char INT8;
typedef char CHAR;

typedef unsigned short int UINT16;
typedef short int INT16;

typedef unsigned int UINT32;
typedef signed int INT32;

/* defines  */
#define SUCCESS			0
#define FAIL 			1
#define NEED_TO_UPGRADE		2
#define NO_NEED_TO_UPGRADE	3

#define UART2_DEV 		"/dev/ttyO2"
#define PTZ_ASSEMBLY_CODE	"Falcon-eye_PTZ.txt"

#define EXPORT_GPIO		"/sys/class/gpio/export"
#define OUT_DIRECTION		"out"
#define SET_LOGIC_HIGH		'1'
#define SET_LOGIC_LOW		'0'

#define SELECTION_GPIO_DIR	"/sys/class/gpio/gpio96/"
#define SELECTION_GPIO_NO	"96"
#define SELECTION_GPIO_STATUS	"/sys/class/gpio/gpio96/direction"
#define SELECTION_GPIO_VALUE	"/sys/class/gpio/gpio96/value"

#define RESET_GPIO_DIR		"/sys/class/gpio/gpio99/"
#define RESET_GPIO_NO		"99"
#define RESET_GPIO_STATUS	"/sys/class/gpio/gpio99/direction"
#define RESET_GPIO_VALUE	"/sys/class/gpio/gpio99/value"

#define TEST_GPIO_DIR		"/sys/class/gpio/gpio100/"
#define TEST_GPIO_NO		"100"
#define TEST_GPIO_STATUS	"/sys/class/gpio/gpio100/direction"
#define TEST_GPIO_VALUE		"/sys/class/gpio/gpio100/value"

enum {
      DETECT_PTZ = 3,
      READ_PTZ_VR = 10,
};


#endif
