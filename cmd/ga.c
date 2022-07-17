#include <common.h>
#include <command.h>
#include <linux/delay.h>



static int do_test_func(struct cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[]) 
{
    volatile u32 *port1 = (u32*)0x30210000;

    // user led is connected to port 1 6th pin

    printf("Testing the customized function\n");


    for(u32 n = 0; n < 10; n++)
    {
        printf("Toggling the LED...\n");
        *port1 = *port1 ^ 0x00000040;
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
