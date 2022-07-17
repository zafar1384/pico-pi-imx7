#include <common.h>
#include <command.h>
#include <linux/delay.h>

#define    PORT_READ(addr)         *((volatile u32 *)(addr)) 
#define    PORT_WRITE(addr, val)   *((volatile u32 *)(addr)) = val      

static int do_test_func(struct cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[]) 
{
    volatile u32 *port1DataReg = (u32*)0x30210000;
    volatile u32 *port1DirReg  = (u32*)0x30210000;
    u32 configVal = 0;

    // user led is connected to port 1 6th pin

    printf("Testing the customized function\n");

    configVal = PORT_READ(*port1DirReg);
    configVal = configVal | 0x00000040;
    PORT_WRITE( *port1DirReg, configVal );
    for(u32 n = 0; n < 10; n++)
    {
        printf("Toggling the LED...\n");
        *port1DataReg = *port1DataReg ^ 0x00000040;
        mdelay(2000);
    }




    return 0;
}


// command to be addded
/* -------------------------------------------------------------------- */

U_BOOT_CMD(
        ga,  1,      0,      do_test_func,
        "print \"Hello world!\"",
        "\n    - print \"Hello world!\""
)
