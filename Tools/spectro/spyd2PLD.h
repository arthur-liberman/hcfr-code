
/* Spyder 2 Colorimeter Xilinx XCS05XL firmware pattern */
/* needs to be transfered here for the instrument to work. */

/* The end user should see the oeminst utility.*/

static unsigned int pld_size = 0x11223344;   /* Endian indicator */
static unsigned int pld_space = 6824;
static unsigned char pld_bytes[6824] = "XCS05XL firmware pattern";	/* Magic number */

