#include "monitor/monitor.h"
#include "monitor/expr.h"
#include "monitor/watchpoint.h"
#include "nemu.h"

#include <stdlib.h>
#include <ctype.h>
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

static int cmd_si(char *args) {
	int n = 1;
	if (args != NULL) {
		char *end;
		long v = strtol(args, &end, 10);
		if (end == args || v <= 0) {
			printf("si: usage: si [N] (N must be a positive integer)\n");
			return 0;
		}
		n = (int)v;
	}
	cpu_exec(n);
	return 0;
}

static void print_eflags(uint32_t val) {
	printf("eflags  0x%08x   [", val);
	if (val & 0x0001) printf(" CF");
	if (val & 0x0004) printf(" PF");
	if (val & 0x0010) printf(" AF");
	if (val & 0x0040) printf(" ZF");
	if (val & 0x0080) printf(" SF");
	if (val & 0x0100) printf(" TF");
	if (val & 0x0200) printf(" IF");
	if (val & 0x0400) printf(" DF");
	if (val & 0x0800) printf(" OF");
	printf(" ]\n");
}

static int cmd_info(char *args) {
	char *sub = strtok(NULL, " \t");
	if (sub == NULL) {
		printf("info: usage: info r (print registers)\n");
		return 0;
	}
	if (strcmp(sub, "r") == 0) {
		printf("eax     0x%08x   %d\n", cpu.eax, cpu.eax);
		printf("ecx     0x%08x   %d\n", cpu.ecx, cpu.ecx);
		printf("edx     0x%08x   %d\n", cpu.edx, cpu.edx);
		printf("ebx     0x%08x   %d\n", cpu.ebx, cpu.ebx);
		printf("esp     0x%08x   %d\n", cpu.esp, cpu.esp);
		printf("ebp     0x%08x   %d\n", cpu.ebp, cpu.ebp);
		printf("esi     0x%08x   %d\n", cpu.esi, cpu.esi);
		printf("edi     0x%08x   %d\n", cpu.edi, cpu.edi);
		printf("eip     0x%08x\n", cpu.eip);
		print_eflags(cpu.eflags.val);
	}
	else {
		printf("info: unknown subcommand '%s'\n", sub);
	}
	return 0;
}

static int cmd_x(char *args) {
	int count = 8;
	int width = 4;
	char fmt = 'x';
	uint32_t addr = 0;

	if (args == NULL) {
		printf("x: usage: x /[NFU] EXPR   (simplified: EXPR is a hex number like 0x100000)\n");
		return 0;
	}

	char *p = args;
	while (*p == ' ' || *p == '\t') p++;

	if (*p == '/') {
		p++;
		char *q = p;
		while (*q && *q != ' ' && *q != '\t') q++;
		int nread = 0;
		char a = 0, b = 0, c = 0;
		sscanf(p, "%d%c%c%n", &count, &a, &b, &nread);
		if (nread == 0) {
			nread = 0;
			if (sscanf(p, "%c%c%c%n", &a, &b, &c, &nread) >= 1) {
				count = 1;
			}
		}
		if (a && isalpha((unsigned char)a)) {
			if (b && isalpha((unsigned char)b)) {
				c = b; b = a; a = 0;
			}
		}
		char sz = 0, f = 0;
		if (a && !isdigit((unsigned char)a)) { sz = a; f = b; }
		else { sz = b; f = c; }

		switch (sz) {
			case 'b': width = 1; break;
			case 'h': width = 2; break;
			case 'w': width = 4; break;
			case 'g': width = 8; break;
			case 0:   width = 4; break;
			default:
				printf("x: unsupported size '%c' (use b/h/w/g)\n", sz);
				return 0;
		}
		if (f) fmt = f;
		if (!isalpha((unsigned char)fmt)) fmt = 'x';
		p = q;
	}

	while (*p == ' ' || *p == '\t') p++;
	char *end;
	unsigned long v = strtoul(p, &end, 16);
	addr = (uint32_t)v;

	if (count <= 0) count = 1;

	int per_line = 16 / (width ? width : 1);
	if (per_line < 1) per_line = 1;

	int i;
	for (i = 0; i < count; i++) {
		uint32_t val;

		if (i % per_line == 0) {
			printf("0x%08x:  ", addr);
		}

		/* 'g' (8 bytes) is not supported by swaddr_read() in the DEBUG
		 * build (it only accepts 1/2/4). Read it as two 4-byte halves. */
		if (width == 8) {
			uint32_t lo = swaddr_read(addr, 4);
			uint32_t hi = swaddr_read(addr + 4, 4);
			switch (fmt) {
				case 'd': printf("%20lld ", (long long)((int64_t)hi << 32 | lo)); break;
				case 'u': printf("%20llu ", (unsigned long long)((uint64_t)hi << 32 | lo)); break;
				default:  printf("%016llx ", (unsigned long long)((uint64_t)hi << 32 | lo)); break;
			}
		}
		else {
			val = swaddr_read(addr, width);
			switch (fmt) {
				case 'd':
					switch (width) {
						case 1: printf("%10d ", (int32_t)(int8_t)val); break;
						case 2: printf("%10d ", (int32_t)(int16_t)val); break;
						case 4: printf("%10d ", (int32_t)val); break;
						default: printf("%10u ", val);
					}
					break;
				case 'u':
					switch (width) {
						case 1: printf("%10u ", (uint8_t)val); break;
						case 2: printf("%10u ", (uint16_t)val); break;
						case 4: printf("%10u ", val); break;
						default: printf("%10u ", val);
					}
					break;
				case 'c':
					printf("%c ", (val >= 32 && val < 127) ? val : '.');
					break;
				case 'x':
				default:
					switch (width) {
						case 1: printf("%02x ", (uint8_t)val); break;
						case 2: printf("%04x ", (uint16_t)val); break;
						case 4: printf("%08x ", val); break;
						default: printf("%08x ", val);
					}
			}
		}

		addr += width;
		if ((i + 1) % per_line == 0) {
			uint32_t line_start = addr - per_line * width;
			int j;
			printf(" |");
			for (j = 0; j < per_line * width; j++) {
				uint8_t b = (uint8_t)swaddr_read(line_start + j, 1);
				putchar((b >= 32 && b < 127) ? b : '.');
			}
			printf("|\n");
		}
	}
	if (i % per_line != 0) {
		int left = per_line - (i % per_line);
		int j;
		for (j = 0; j < left; j++) {
			switch (fmt) {
				case 'd':
				case 'u': printf("           "); break;
				case 'c': printf("  "); break;
				default:
					if (width == 1) printf("   ");
					else if (width == 2) printf("     ");
					else printf("           ");
			}
		}
		uint32_t line_start = addr - (i % per_line) * width;
		printf(" |");
		for (j = 0; j < (i % per_line) * width; j++) {
			uint8_t b = (uint8_t)swaddr_read(line_start + j, 1);
			putchar((b >= 32 && b < 127) ? b : '.');
		}
		printf("|\n");
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
	{ "si", "Single step: si [N] - execute N instructions (default N=1)", cmd_si },
	{ "info", "Print runtime information: info r - print registers", cmd_info },
	{ "x", "Examine memory: x /[N][s][f] ADDR   s=b/h/w/g  f=x/d/u/c", cmd_x },

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
