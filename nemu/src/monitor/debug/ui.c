#include "monitor/monitor.h"
#include "monitor/expr.h"
#include "monitor/watchpoint.h"
#include "nemu.h"

#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>

void cpu_exec(uint32_t);

/* We use the `readline' library to provide more flexibility to read from stdin. */
char* rl_gets() {
	static char *line_read = NULL;

	if (line_read) {
		free(line_read);
		line_read = NULL;
	}

	line_read = readline("(nemu) ");

	if (line_read && *line_read) {
		add_history(line_read);
	}

	return line_read;
}

static int cmd_c(char *args) {
	cpu_exec(-1);
	return 0;
}

static int cmd_q(char *args) {
	return -1;
}

/* si [N]: execute N instructions, where N defaults to 1 */
static int cmd_si(char *args) {
	int n = 1;
	char *arg = strtok(NULL, " ");

	if(arg != NULL) {
		n = atoi(arg);
		if(n <= 0) {
			printf("Invalid step number '%s'\n", arg);
			return 0;
		}
	}

	cpu_exec(n);
	return 0;
}

/* info r: print the values of the registers */
static int cmd_info(char *args) {
	char *sub = strtok(NULL, " ");

	if(sub == NULL) {
		printf("Usage: info r\n");
		return 0;
	}

	if(strcmp(sub, "r") == 0) {
		printf("eax 0x%08x %u\n", cpu.eax, cpu.eax);
		printf("ecx 0x%08x %u\n", cpu.ecx, cpu.ecx);
		printf("edx 0x%08x %u\n", cpu.edx, cpu.edx);
		printf("ebx 0x%08x %u\n", cpu.ebx, cpu.ebx);
		printf("esp 0x%08x %u\n", cpu.esp, cpu.esp);
		printf("ebp 0x%08x %u\n", cpu.ebp, cpu.ebp);
		printf("esi 0x%08x %u\n", cpu.esi, cpu.esi);
		printf("edi 0x%08x %u\n", cpu.edi, cpu.edi);
	}
	else {
		printf("Unknown info subcommand '%s'\n", sub);
	}

	return 0;
}

/* x N EXPR: print N 4-byte words (in hex) starting from the address EXPR.
 * In this stage EXPR is simplified to be a hexadecimal number only,
 * e.g. "x 10 0x100000".
 */
static int cmd_x(char *args) {
	char *arg1 = strtok(NULL, " ");
	char *arg2 = strtok(NULL, " ");

	if(arg1 == NULL || arg2 == NULL) {
		printf("Usage: x N EXPR   (e.g. x 10 0x100000)\n");
		return 0;
	}

	int n = atoi(arg1);
	if(n <= 0) {
		printf("Invalid number '%s'\n", arg1);
		return 0;
	}

	uint32_t addr = (uint32_t)strtoul(arg2, NULL, 16);

	int i;
	for(i = 0; i < n; i ++) {
		if(i % 4 == 0) {
			printf("0x%08x: ", addr);
		}
		printf("0x%08x", swaddr_read(addr, 4));
		addr += 4;
		if(i % 4 == 3 || i == n - 1) {
			printf("\n");
		}
		else {
			printf(" ");
		}
	}

	return 0;
}

static int cmd_help(char *args);

static struct {
	char *name;
	char *description;
	int (*handler) (char *);
} cmd_table [] = {
	{ "help", "Display informations about all supported commands", cmd_help },
	{ "c", "Continue the execution of the program", cmd_c },
	{ "q", "Exit NEMU", cmd_q },
	{ "si", "si [N] - execute N instructions (default N = 1)", cmd_si },
	{ "info", "info r - print the values of the registers", cmd_info },
	{ "x", "x N EXPR - print N 4-byte words in hex starting from EXPR", cmd_x },

	/* TODO: Add more commands */

};

#define NR_CMD (sizeof(cmd_table) / sizeof(cmd_table[0]))

static int cmd_help(char *args) {
	/* extract the first argument */
	char *arg = strtok(NULL, " ");
	int i;

	if(arg == NULL) {
		/* no argument given */
		for(i = 0; i < NR_CMD; i ++) {
			printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
		}
	}
	else {
		for(i = 0; i < NR_CMD; i ++) {
			if(strcmp(arg, cmd_table[i].name) == 0) {
				printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
				return 0;
			}
		}
		printf("Unknown command '%s'\n", arg);
	}
	return 0;
}

void ui_mainloop() {
	while(1) {
		char *str = rl_gets();
		char *str_end = str + strlen(str);

		/* extract the first token as the command */
		char *cmd = strtok(str, " ");
		if(cmd == NULL) { continue; }

		/* treat the remaining string as the arguments,
		 * which may need further parsing
		 */
		char *args = cmd + strlen(cmd) + 1;
		if(args >= str_end) {
			args = NULL;
		}

#ifdef HAS_DEVICE
		extern void sdl_clear_event_queue(void);
		sdl_clear_event_queue();
#endif

		int i;
		for(i = 0; i < NR_CMD; i ++) {
			if(strcmp(cmd, cmd_table[i].name) == 0) {
				if(cmd_table[i].handler(args) < 0) { return; }
				break;
			}
		}

		if(i == NR_CMD) { printf("Unknown command '%s'\n", cmd); }
	}
}
