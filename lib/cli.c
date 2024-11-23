#include "cli.h"

#include <stdio.h>

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

void cli_init(cli_t *cli, cli_exec_cb exec_cb)
{
    cli->char_cnt = 0;
    cli->exec_cb = exec_cb;
}

void cli_getc(cli_t *cli, char c)
{
    if (cli->char_cnt < cli_max_buflen)
    {
        cli->buffer[cli->char_cnt++] = c;
    }
}

void cli_parse(cli_t *cli)
{
    int cnt;
    uint32_t char_cnt = cli->char_cnt;
    static char msg_buf[cli_max_msg_buflen];

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
        cli_puts(cli, "Too many arguments!\r\n");
        return;
    }
    else
    {
        snprintf(msg_buf, cli_max_msg_buflen, "Number of tokens: %d\r\n", cnt);
        cli_puts(cli, msg_buf);

        cli->exec_cb(cli->token_array, cnt);
        return;
    }
}

