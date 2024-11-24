#ifndef CLI_H
#define CLI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

enum { cli_max_buflen = 256 };
enum { cli_max_msg_buflen = 256 };
enum { cli_max_tokens = 4 };

typedef struct {
    char *name;
    void (*callback)(char const **tokens, int tokens_amount);
} cli_command_t;

typedef struct {
    uint32_t char_cnt;
    cli_command_t const *cmds;
    uint32_t cmds_amount;
    char buffer[cli_max_buflen + 1];
    char const *token_array[cli_max_tokens];
} cli_t;

extern void cli_puts(cli_t *cli, const char *s);

void cli_init(cli_t *cli, cli_command_t const *cmds, uint32_t cmds_amount);
void cli_getc(cli_t *cli, char c);
void cli_parse(cli_t *cli);

#ifdef __cplusplus
}
#endif

#endif /* CLI_H */
