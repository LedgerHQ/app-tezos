/* The stubs that the fuzz build needs, for what is in the excluded
 * src/app_main.c. */

#include <os.h>

/* The quit of the menu comes back to here. Exit code 255 (-1) is an oracle of
 * the framework, which records it as a failed assertion and would end the
 * campaign: this is an ordinary exit. */
void
app_exit(void)
{
    os_sched_exit(0);
}
