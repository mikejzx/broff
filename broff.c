/*
 * roff-to-html.c
 *
 * Generate HTML from Roff-style documents.  Supported macro set is based on
 * the 'ms' macros.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// Output indentation
#if 0
#  define INDENT "    "
#  define INDENT_BASE
#else
#  define INDENT "\t"
#  define INDENT_BASE INDENT INDENT
#endif

static enum command
{
    CMD_NONE = 0,
    CMD_NH,
    CMD_PP,
    CMD_DS,
    CMD_TL,

    // Non-standard
    CMD_LI,
} cmd = CMD_NONE;

static int heading_level;
static bool is_sentence = false;
static char *line;
static ssize_t len;
static char date_str[32] = { 0 };

static bool check_font(const char *restrict const, const char *restrict const);
static bool check_link(void);

static inline void
end_sentence(void)
{
    if (is_sentence) printf("</span>\n");
    is_sentence = false;
}

static inline bool
is_sentence_end(char c)
{
    return c == '.' ||
        c == '!' ||
        c == '?';
}

static void
end_last_cmd(void)
{
    // End of sentence
    end_sentence();

    switch(cmd)
    {
    case CMD_NH:
        printf(INDENT_BASE INDENT
            "</h%d>\n", heading_level);
        break;
    case CMD_PP:
        printf(INDENT_BASE INDENT
            "</p>\n");
        break;
    case CMD_LI:
        printf(INDENT_BASE INDENT
            "</li>\n");
        printf(INDENT_BASE INDENT
            "</ul>\n");
        break;
    case CMD_TL:
        // Also print the date if we parsed one
        if (*date_str)
        {
            printf(INDENT_BASE INDENT INDENT
                "<span style=\"float: right\">%s</span>\n", date_str);
        }
        printf(INDENT_BASE INDENT
            "</header>\n");
        break;
    default: break;
    }
}

int
main(int argc, char *argv[])
{
    FILE *fp = NULL;

    if (argc > 1)
    {
        // Read from file
        fp = fopen(argv[1], "r");
        if (!fp)
        {
            fprintf(stderr, "md-to-html: no such file\n");
            exit(1);
        }
    }
    else
    {
        // Read from stdin
        fp = stdin;
    }

    // Begin article
    printf(INDENT_BASE "<article>\n");

    // Parse
    size_t len_tmp;
    for (line = NULL; (len = getline(&line, &len_tmp, fp)) != -1;)
    {
        // Strip newline
        line[--len] = '\0';
        if (len <= 0) continue;

        if (len == 1 &&
            *line == '.') continue;

        if (strncmp(line, ".\\\"", strlen(".\\\"")) == 0) continue;

        // .DE end display
        if (cmd == CMD_DS &&
            len >= strlen(".DE") &&
            strncmp(line, ".DE", strlen(".DE")) == 0)
        {
            printf(INDENT_BASE INDENT "</pre>\n");
            cmd = CMD_NONE;
            continue;
        }

        // We are in a preformatted block; just keep printing as-is
        if (cmd == CMD_DS)
        {
            printf("\n%s", line);
            continue;
        }

        // .TL title
        if (len >= strlen(".TL") &&
            strncmp(line, ".TL", strlen(".TL")) == 0)
        {
            end_last_cmd();
            cmd = CMD_TL;

            printf(INDENT_BASE INDENT
                "<header>\n");
            continue;
        }
        // .NH Section headings
        if (len >= strlen(".NH") &&
            strncmp(line, ".NH", strlen(".NH")) == 0)
        {
            end_last_cmd();
            cmd = CMD_NH;

            // Integer that follows is heading level
            heading_level = 1;
            if (len > strlen(".NH"))
            {
                heading_level = atoi(line + strlen(".NH"));
            }
            printf(INDENT_BASE INDENT
                "<h%d>\n", heading_level);
            continue;
        }
        // .PP indented paragraph
        if (len >= strlen(".PP") &&
            strncmp(line, ".PP", strlen(".PP")) == 0)
        {
            end_last_cmd();
            cmd = CMD_PP;

            printf(INDENT_BASE INDENT "<p>\n");
            continue;
        }
        // .DS begin display
        if (len >= strlen(".DS") &&
            strncmp(line, ".DS", strlen(".DS")) == 0)
        {
            end_last_cmd();
            cmd = CMD_DS;
            printf(INDENT_BASE INDENT "<pre>");
            continue;
        }
        // .LI unordered list item
        if (len >= strlen(".LI") &&
            strncmp(line, ".LI", strlen(".LI")) == 0)
        {
            if (cmd != CMD_LI)
            {
                end_last_cmd();
                printf(INDENT_BASE INDENT "<ul>\n");
            }
            else
            {
                end_sentence();
                printf(INDENT_BASE INDENT "</li>\n");
            }
            cmd = CMD_LI;

            printf(INDENT_BASE INDENT "<li>\n");
            continue;
        }

        // .DA date
        if (len > strlen(".DA") &&
            strncmp(line, ".DA", strlen(".DA")) == 0)
        {
            strncpy(date_str, line + strlen(".DA") + 1, sizeof(date_str));
            continue;
        }

        // Print the content on per-sentence basis; in spans for nice sentence
        // spacing.
        if (!is_sentence)
        {
            printf(INDENT_BASE INDENT INDENT "<span class=\"sntc\">");
            is_sentence = true;
        }
        else
        {
            // Put whitespace between words of the sentence
            printf(" ");
        }

        // .B bold font
        // .I italic font
        if (check_font(".B", "b")) continue;
        if (check_font(".I", "i")) continue;


        // .LNK link
        if (check_link()) continue;

        // Print out the text
        printf("%s", line);

        // Detect end of sentence
        if (is_sentence_end(line[len - 1]))
        {
            end_sentence();
        }
    }
    // End last command
    end_last_cmd();

    // Close article
    printf(INDENT_BASE "</article>\n");

    // Close file
    if (argc > 1) fclose(fp);

    return 0;
}

static bool
check_font(
    const char *restrict const roff_cmd,
    const char *restrict const tag)
{
    if (len < strlen(roff_cmd) ||
        strncmp(line, roff_cmd, strlen(roff_cmd)) != 0) return false;

    /*
     * A font command appears like so:
     *   .B "Arg 1" "arg 2" "arg 3"
     * However, it can also appear like this
     *   .B  Test 1 2
     * Or
     *   .B  "Testing" 1 "2 test"
     * The idea is that each argument can either be in quotations, or not be
     * and end at the first space
     */

    int roff_cmd_len = strlen(roff_cmd);

    struct
    {
        const char *s, *e;
    } args[3] = { 0 };

    const char *x = line + roff_cmd_len;
    const char *e = NULL;

    for (int i = 0; i < 3; ++i)
    {
        // Move to the next non-whitespace char
        x += strspn(x, " ");
        if (*x == '\"')
        {
            // This argument is in quotes, so it ends at the next quote
            ++x;
            e = x + strcspn(x, "\"");

            args[i].s = x;
            args[i].e = e;

            x = e + 1;
        }
        else
        {
            // Argument ends at the next space
            e = x + strcspn(x, " ");

            args[i].s = x;
            args[i].e = e;

            x = e + 1;
        }

        if (x >= line + len) break;
    }

    // Print the immediate prefix, if any
    if (args[2].s)
    {
        printf("%.*s", (int)(args[2].e - args[2].s), args[2].s);
    }

    // Print the actual content within the tags
    printf("<%s>%.*s</%s>", tag, (int)(args[0].e - args[0].s), args[0].s, tag);

    // Print the immediate suffix, if any
    if (args[1].s)
    {
        printf("%.*s", (int)(args[1].e - args[1].s), args[1].s);

        // If the suffix ends on sentence
        if (is_sentence_end(*(args[1].e - 1))) end_sentence();
    }
    else if (args[0].e)
    {
        // If the content itself ends on sentence
        if (is_sentence_end(*(args[0].e - 1))) end_sentence();
    }

    return true;
}

static bool
check_link(void)
{
    static const char *const LNK_CMD = ".H";
    static const int LNK_CMD_LEN = strlen(LNK_CMD);

    if (len < LNK_CMD_LEN ||
        strncmp(line, LNK_CMD, LNK_CMD_LEN) != 0) return false;

    /*
     * H commands follows this format
     *
     * .H <link> "text to describe link" "postfix" "suffix"
     */

    struct
    {
        const char *s, *e;
    } args[4] = { 0 };

    const char *x = line + LNK_CMD_LEN;
    const char *e = NULL;

    // TODO this is duplicated from check_font; try move abstract it away or
    // something
    for (int i = 0; i < 4; ++i)
    {
        // Move to the next non-whitespace char
        x += strspn(x, " ");
        if (*x == '\"')
        {
            // This argument is in quotes, so it ends at the next quote
            ++x;
            e = x + strcspn(x, "\"");

            args[i].s = x;
            args[i].e = e;

            x = e + 1;
        }
        else
        {
            // Argument ends at the next space
            e = x + strcspn(x, " ");

            args[i].s = x;
            args[i].e = e;

            x = e + 1;
        }

        if (x >= line + len) break;
    }

    // Print the immediate prefix, if any
    if (args[3].s)
    {
        printf("%.*s", (int)(args[3].e - args[3].s), args[3].s);
    }

    // Print the actual content within the tags
    printf("<a href=\"%.*s\">%.*s</a>",
        (int)(args[0].e - args[0].s), args[0].s,
        (int)(args[1].e - args[1].s), args[1].s);

    // Print the immediate suffix, if any
    if (args[2].s)
    {
        printf("%.*s", (int)(args[2].e - args[2].s), args[2].s);

        // If the suffix ends on sentence
        if (is_sentence_end(*(args[2].e - 1))) end_sentence();
    }
    else if (args[1].e)
    {
        // If the content itself ends on sentence
        if (is_sentence_end(*(args[1].e - 1))) end_sentence();
    }

    return true;
}
