#include <common.h>
#include <command.h>




static int do_test_func(cmd_tbl_t * cmdtp, int flag, int argc, char * const argv[]) 
{
	printf("Testing the customized function\n");
	return 0;
}


// command to be addded
/* -------------------------------------------------------------------- */

U_BOOT_CMD(
        ga,  1,      0,      do_test_func,
        "print \"Hello world!\"",
        "\n    - print \"Hello world!\""
)
