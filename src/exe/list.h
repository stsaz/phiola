/** phiola: executor: 'list' command
2023, Simon Zolin */

#include <exe/list-create.h>
#include <exe/list-heal.h>
#include <exe/list-sort.h>

static int list_help()
{
	help_info_write("\
Process playlist files\n\
\n\
    `phiola list` COMMAND [OPTIONS]\n\
\n\
COMMAND:\n\
\n\
  `create`            Create playlist file\n\
  `heal`              Heal playlist\n\
  `sort`              Sort playlist\n\
\n\
Use 'phiola list COMMAND -h' for more info.\n\
");
	x->exit_code = 0;
	return 1;
}

const struct ffarg cmd_list_args[] = {
	{ "-help",		'1',	list_help },
	{ "create",		'{',	list_create_init },
	{ "heal",		'{',	list_heal_init },
	{ "sort",		'{',	list_sort_init },
	{}
};
