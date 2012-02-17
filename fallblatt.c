#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>

#define FWITK_CMD_MASK (1 << 7)
#define FWITK_CMD_ADDR7 (1 << 6)
#define FWITK_CMD_CODE7 (1 << 5)

#define FWITK_CMD_STARTALL          0x1
#define FWITK_CMD_SETEMPTY          0x2
#define FWITK_CMD_SWRESET           0x3
#define FWITK_CMD_STATUSQUERY       0x4
#define FWITK_CMD_CODEQUERY         0x5
#define FWITK_CMD_LOCK              0x6
#define FWITK_CMD_UNLOCK            0x7
#define FWITK_CMD_SETCODE           0x8
#define FWITK_CMD_CALIBRATE_AUTO    0x9
#define FWITK_CMD_CALIBRATE_MANUAL  0xa
#define FWITK_CMD_FINISHPRINTLIST   0xb
#define FWITK_CMD_READCALIBRATION   0xc
#define FWITK_CMD_STARTPRINTLIST    0xd
#define FWITK_CMD_ERASEPRINTLIST    0xe

extern uint8_t page_lookup[];

typedef struct {
    int fd;
} FBPORT;

int fbport_init(FBPORT* fb, const char* ttyname)
{
    fb->fd = open(ttyname, O_RDWR | O_NONBLOCK);
	if (fb->fd <= 0) {
		perror("open");
		return 1;
	}
	
	
	// OSX Hack: set fd to blocking again:
	int flags = fcntl(fb->fd, F_GETFL, 0);
    fcntl(fb->fd, F_SETFL, flags | ~O_NONBLOCK);

	struct termios mode;
	tcgetattr(fb->fd, &mode);

	mode.c_iflag = IGNPAR;
	mode.c_oflag = 0;
	mode.c_cflag = CLOCAL | CREAD | CS8 | B4800 | PARENB | CREAD;
	mode.c_lflag = 0;
	mode.c_cc [VMIN] = 0;
	mode.c_cc [VTIME] = 0;

    tcflush(fb->fd, TCIOFLUSH);
	tcsetattr(fb->fd, TCSANOW, &mode);
	
	
}

void fbport_deinit(FBPORT* fb)
{
    close(fb->fd);
}

void fbport_writebyte(FBPORT* fb, uint8_t byte)
{
    // TODO: error handling    
    int ret = write(fb->fd, &byte, 1);
    
    if (ret != 1) {
        perror("write");
    }
}

int fbport_readbyte(FBPORT* fb, uint8_t* byte)
{
    // TODO: error handling
    int ret = read(fb->fd, byte, 1);
    
    if (ret < 0) {
        perror("read");
        return 0;
    }
    
    return ret;
}

void fbport_cmd(FBPORT* fb, uint8_t cmdnum)
{
    fbport_writebyte(fb, FWITK_CMD_MASK | cmdnum);
}

void fbport_cmd_addr(FBPORT* fb, int cmdnum, int addr)
{
    int cmd = FWITK_CMD_MASK | cmdnum;
    if (addr > 127) {
        cmd |= FWITK_CMD_ADDR7;
    }
    
    fbport_writebyte(fb, cmd);
    fbport_writebyte(fb, addr & 0x7f);
}

void fbport_set(FBPORT* fb, int addr, int code)
{
    
    int cmd = FWITK_CMD_MASK | FWITK_CMD_SETCODE;
    if (addr > 127) {
        cmd |= FWITK_CMD_ADDR7;
    }
    
    if (code > 127) {
        cmd |= FWITK_CMD_CODE7;
    }
    
    fbport_writebyte(fb, cmd);
    fbport_writebyte(fb, addr & 0x7f);
    fbport_writebyte(fb, code & 0x7f);
}

void load_printedlist(FBPORT* fb, int addr)
{
    // Bedruckungsliste laden
    
    fbport_cmd_addr(fb, FWITK_CMD_STARTPRINTLIST, addr);

	int i;
	for (i = 0; i < 50; i++) {
		uint8_t page = i;
		uint8_t code = i + 32;
		
	    fbport_writebyte(fb, page);
	    fbport_writebyte(fb, code);

        uint8_t readbyte;
        fbport_readbyte(fb, &readbyte);
        
        if (readbyte != code) {
            puts("Warning: Read back wrong page code");
        }
        
        printf("Programmed Page %d at code %d (recv %d)\n", page, code, readbyte);
    }
	
    fbport_cmd_addr(fb, FWITK_CMD_FINISHPRINTLIST, addr);

}


void scan_devices(FBPORT* fb)
{
    int i;
    for (i = 0; i < 256; i++) {
        fbport_cmd_addr(fb, FWITK_CMD_STATUSQUERY, i);
        
        usleep(10000);
        uint8_t status;
        if (fbport_readbyte(fb, &status)) {
            printf("Found Device at address %d\n", i);
            
            if (!(status & 0xc)) {
                puts("Warning: received invalid status response");
            }
            
            printf("\tEmpty: %d\tResetflag: %d\tLocked: %d\tError code: %x", 
                (status >> 6) & 1, (status >> 5) & 1, (status >> 4) & 1, status & 0x0f);
            
        }        
    }
}

void set_string(FBPORT* fb, uint8_t address, char* message)
{
    int i;
    int len = strlen(message);
    
    if (len >= 255) {
        puts("Warning: set message too long");
        message[255] = 0;
        len = 255;        
    }
    
    for (i = address; i < len; i++) {
        int msg = message[i];
        if (msg < 32 || msg > 127) {
            puts("Warning: Invalid char");
            msg = '*';
        }
        
        msg = toupper(msg);        
        int code = page_lookup[msg] + 32;
        
        fbport_set(fb, address, code);
    }
    
    fbport_cmd(fb, FWITK_CMD_STARTALL);
}

void print_help()
{
    puts("Usage:    Check the source TROLOLOL");
    exit(0);
}

static const char *optString = ":ap:h";

static const struct option longOpts[] = {
    { "port", required_argument, NULL, 'p' },
    { "address", required_argument, NULL, 'a' },
    { "help", no_argument, NULL, 'h' },
    { NULL, no_argument, NULL, 0 }
};

int main(int argc, char** argv) {
    const char* ttyport = "/dev/ttyS0";
    int address = 0;
    
    int longIndex;
    int opt = getopt_long( argc, argv, optString, longOpts, &longIndex );
    
    while( opt != -1 ) {
        switch( opt ) {
            case 'a':
                address = atoi(optarg);
                break;
            case 'h':
                print_help();
                break;                
            case 'p':
                ttyport = optarg;
                break;
            case 0:
                puts(longOpts[longIndex].name );/* long option without a short arg */
//                if( strcmp( "randomize", longOpts[longIndex].name ) == 0 ) {
//                    globalArgs.randomized = 1;
//                }
                break;
                
            default:
                /* You won't actually get here. */
                break;
        }
        
        opt = getopt_long( argc, argv, optString, longOpts, &longIndex );
    }
    
    if (argc - optind < 1) {
        print_help();
    }
    
	FBPORT fbport;
    int ret = fbport_init(&fbport, ttyport);
    if (ret != 0) {
        printf("Error opening tty port %s\n", ttyport);
        exit(1);
    }
    
    if (strcmp(argv[optind], "list") == 0) {
        scan_devices(&fbport);
    } else if (strcmp(argv[optind], "program") == 0) {
        if (address == -1) {
            print_help();
        }
        
        load_printedlist(&fbport, address);
    } else if (strcmp(argv[optind], "reset") == 0) {
        fbport_cmd(&fbport, FWITK_CMD_SETEMPTY);
    } else if (strcmp(argv[optind], "set") == 0) {
        if (argc - optind < 2) {
            print_help();
        }
        
        set_string(&fbport, address, argv[optind + 1]);
    }


    
    fbport_deinit(&fbport);
}
