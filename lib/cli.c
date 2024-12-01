#include "cli.h"

#include <stdio.h>
#include <string.h>

static int split(char const *input_str,
                 int char_count_limit,
                 int tokens_max_count,
                 char const **token_array)
{
    int tokens_count = 0;
    int char_idx = 0;

    while (1)
    {
        while ((input_str[char_idx] == ' ') && (char_idx < char_count_limit))
            char_idx++;

        if (char_idx >= char_count_limit)
            return tokens_count;

        if (tokens_count >= tokens_max_count)
            return -1;

        token_array[tokens_count++] = input_str + char_idx;

        while ((input_str[char_idx] != ' ') && (char_idx < char_count_limit))
            char_idx++;

        if (char_idx >= char_count_limit)
            return tokens_count;
    }

    return tokens_count;
}

void cli_init(cli_t *cli, cli_command_t const *cmds, uint32_t cmds_amount)
{
    cli->char_cnt = 0;

    cli->cmds = cmds;
    cli->cmds_amount = cmds_amount;
}

void cli_push_char(cli_t *cli, char c)
{
    if (cli->char_cnt < cli_max_inbuf_len)
    {
        cli->input_buffer[cli->char_cnt++] = c;
    }
}

static void cli_process_internal(cli_t *cli)
{
    int cnt;
    int i;
    uint32_t char_cnt = cli->char_cnt;

    cli->char_cnt = 0;
    cli->input_buffer[char_cnt] = '\0';

    if (char_cnt == 0)
    {
        return;
    }

    cnt = split(cli->input_buffer, char_cnt, cli_max_tokens, cli->token_array);

    if (cnt == -1)
    {
        cli_puts(cli, "Too many tokens!\r\n");
        return;
    }

    if (0 == strncmp(cli->token_array[0], CLI_HELP_CMD_NAME, strlen(CLI_HELP_CMD_NAME)))
    {
        for (i = 0; i < cli->cmds_amount; i++)
        {
            cli_puts(cli, cli->cmds[i].name);
            cli_puts(cli, "\t");
            cli_puts(cli, cli->cmds[i].description);
        }

        cli_puts(cli, CLI_HELP_CMD_NAME);
        cli_puts(cli, "\t");
        cli_puts(cli, CLI_HELP_CMD_DESCRIPTION);

        return;
    }

    for (i = 0; i < cli->cmds_amount; i++)
    {
        if (0 == strncmp(cli->token_array[0], cli->cmds[i].name, strlen(cli->cmds[i].name)))
        {
            uint32_t msglen;

            if (cli_success != cli->cmds[i].worker(cli->token_array, cnt, cli->output_buffer, &msglen))
            {
                cli_puts(cli, "Usage: ");
                cli_puts(cli, cli->cmds[i].name);
                cli_puts(cli, " ");
                cli_puts(cli, cli->cmds[i].usage);
            }
            else
            {
                if (msglen != 0)
                {
                    cli_puts(cli, cli->output_buffer);
                }
            }

            return;
        }
    }

    cli_puts(cli, "Unknown command!\r\n");
    return;
}

void cli_process(cli_t *cli)
{
    cli_process_internal(cli);
    cli_puts(cli, CLI_PROMPT);
}
