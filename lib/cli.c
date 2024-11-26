#include "cli.h"

#include <stdio.h>
#include <string.h>

static int split(char *s, int limit, int tok_max_cnt, char const **token_array)
{
    int i = 0;
    int idx = 0;

    while (1)
    {
        while ((s[idx] == ' ') && (idx < limit))
            idx++;

        if (idx >= limit)
            return i;

        if (i >= tok_max_cnt)
            return -1;

        token_array[i++] = s + idx;

        while ((s[idx] != ' ') && (idx < limit))
            idx++;

        if (idx >= limit)
            return i;
    }

    return i;
}

void cli_init(cli_t *cli, cli_command_t const *cmds, uint32_t cmds_amount)
{
    cli->char_cnt = 0;

    cli->cmds = cmds;
    cli->cmds_amount = cmds_amount;
}

void cli_getc(cli_t *cli, char c)
{
    if (cli->char_cnt < cli_max_buflen)
    {
        cli->buffer[cli->char_cnt++] = c;
    }
}

void cli_process(cli_t *cli)
{
    int cnt;
    uint32_t char_cnt = cli->char_cnt;

    cli->char_cnt = 0;
    cli->buffer[char_cnt] = '\0';

    if (char_cnt == 0)
    {
        cli_puts(cli, "Nothing to do!\r\n");
        return;
    }

    cnt = split(cli->buffer, char_cnt, cli_max_tokens, cli->token_array);

    if (cnt == -1)
    {
        cli_puts(cli, "Too many tokens!\r\n");
        return;
    }
    else
    {
        int i;

        for (i = 0; i < cli->cmds_amount; i++)
        {
            if (0 == strncmp(cli->token_array[i], cli->cmds[i].name, strlen(cli->cmds[i].name)))
            {
                uint32_t msglen;

                if (cli_success != cli->cmds[i].worker(cli->token_array, cnt, cli->msg_buffer, &msglen))
                {
                    cli_puts(cli, "Usage: ");
                    cli_puts(cli, cli->cmds[i].usage);
                }
                else
                {
                    if (msglen != 0)
                    {
                        cli_puts(cli, cli->msg_buffer);
                    }
                }

                return;
            }
        }

        cli_puts(cli, "Unknown command!\r\n");
        return;
    }
}

