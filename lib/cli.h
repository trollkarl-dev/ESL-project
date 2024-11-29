#ifndef CLI_H
#define CLI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define CLI_PROMPT "\e[32mcli@colorpicker$\e[0m "
#define CLI_HELP_CMD_NAME "help"
#define CLI_HELP_CMD_DESCRIPTION "Print this message\r\n"

enum { cli_max_inbuf_len = 256 };
enum { cli_max_outbuf_len = 256 };
enum { cli_max_tokens = 4 };

typedef enum {
    cli_success,
    cli_error
} cli_result_t;

typedef struct {
    char const *name;
    char const *usage;
    char const *description;

    cli_result_t (*worker)(char const **tokens,
                           int tokens_amount,
                           char *msg,
                           uint32_t *msglen);
} cli_command_t;

typedef struct {
    uint32_t char_cnt;
    cli_command_t const *cmds;
    uint32_t cmds_amount;
    char input_buffer[cli_max_inbuf_len + 1];
    char output_buffer[cli_max_outbuf_len + 1];
    char const *token_array[cli_max_tokens];
} cli_t;

extern void cli_puts(cli_t *cli, const char *s);

void cli_init(cli_t *cli, cli_command_t const *cmds, uint32_t cmds_amount);
void cli_push_char(cli_t *cli, char c);
void cli_process(cli_t *cli);

#ifdef __cplusplus
}
#endif

#endif /* CLI_H */
