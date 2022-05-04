/*
 * roff-to-html.c
 *
 * Generate HTML from Roff-style documents.  Supported macro set is based on
 * the 'ms' macros.
 */

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static bool check_font(
    const char *restrict const, const char *restrict const, bool);
static bool check_link(void);
static bool check_img(void);

static inline void
end_sentence(void)
{
    if (is_sentence) printf("</span>\n");
    is_sentence = false;
}

static inline bool
is_sentence_end(const char *s, int len)
{
#define SENTENCE_END_CHARS ".?!"

    // First simply check thelast character
    const char *c = &s[len - 1];
    if (strchr(SENTENCE_END_CHARS, *c) != NULL) return true;

    // If full stop preceeds a certain set of punctuation, then we can call it
    // a sentence.
    for (; c >= s &&
        strchr(SENTENCE_END_CHARS "()[]`'\"", *c) != NULL;
        --c)
    {
        if (strchr(SENTENCE_END_CHARS, *c) == NULL) return true;
    }

    return false;
#undef SENTENCE_END_CHARS
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

static void
print_escaped(const char *line, int l)
{
#if 0
    // Verbatim print of content
    printf("%s", line);
#else
    /*
     * Check for any in-text escape sequences to parse, and print.
     * Probably quite slow, but we really need to do this.
     */
    int len_remain;
    for (const char *c = line; c < line + l; ++c)
    {
        len_remain = line + l - c;

    #define HANDLE_ESC(esc, sub) \
        if (len_remain >= strlen((esc)) && \
            strncmp(c, (esc), strlen((esc))) == 0) \
        { \
            if ((sub)) printf((sub)); \
            c += strlen((esc)) - 1; \
            continue; \
        }

        // Roff escapes
        HANDLE_ESC("\\&",   NULL);
        HANDLE_ESC("\\~",   "&nbsp;");
        HANDLE_ESC("\\(em", "&mdash;");
        HANDLE_ESC("\\(lq", "&ldquo;");
        HANDLE_ESC("\\(rq", "&rdquo;");
        HANDLE_ESC("\\(oq", "&lsquo;");
        HANDLE_ESC("\\(cq", "&rsquo;");
        HANDLE_ESC("\\(aq", "'");
        HANDLE_ESC("\\(dq", "\"");

        // TeX style typographer quotes; must be in this order
        HANDLE_ESC("``", "&ldquo;");
        HANDLE_ESC("''", "&rdquo;");
        HANDLE_ESC("`",  "&lsquo;");
        HANDLE_ESC("'",  "&rsquo;");

        // HTML escapes (must run last because of the ampersand one)
        HANDLE_ESC("<", "&lt;");
        HANDLE_ESC(">", "&gt;");
        HANDLE_ESC("&", "&amp;");
        HANDLE_ESC("...", "&hellip;");

        if (ispunct(*c) && strchr(",.!?;:'\"", *c) == NULL)
        {
            // Escape characters that aren't alphanumeric and are not a
            // certain set of punctuation
            printf("&#%d;", *c);
            continue;
        }

        // No escape here, just print out this content normally
        printf("%c", *c);
    }
#endif
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
            fprintf(stderr, "broff: no such file\n");
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
            //printf("\n%s", line);
            printf("\n", line);

            for (const char *c = line; *c; ++c)
            {
                if (ispunct(*c) && strchr(",.!?;:'\"", *c) == NULL)
                {
                    // Escape characters that aren't alphanumeric and are not a
                    // certain set of punctuation
                    printf("&#%d;", *c);
                    continue;
                }
                printf("%c", *c);
            }
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

            printf(INDENT_BASE INDENT "<p class=\"sentspc\">\n");
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

            printf(INDENT_BASE INDENT "<li class=\"sentspc\">\n");
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

        // .IM image check
        if (check_img()) continue;

        // .B bold font
        // .I italic font
        // .F fixed font
        if (check_font(".B", "b", true)) continue;
        if (check_font(".I", "i", true)) continue;
        if (check_font(".F", "code", false)) continue;

        // .H link check
        if (check_link()) continue;

        // Print out the text content
        print_escaped(line, len);

        // Detect end of sentence
        if (is_sentence_end(line, len))
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
    const char *restrict const tag,
    bool sentspc)
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
    if (args[2].s) print_escaped(args[2].s, (int)(args[2].e - args[2].s));

    // Print the actual content within the tags
    printf("<%s>", tag);
    print_escaped(args[0].s, (int)(args[0].e - args[0].s));
    printf("</%s>", tag);

    // Print the immediate suffix, if any
    if (args[1].s)
    {
        print_escaped(args[1].s, (int)(args[1].e - args[1].s));

        // If the suffix ends on sentence
        if (sentspc && is_sentence_end(args[1].s, (int)(args[1].e - args[1].s)))
            end_sentence();
    }
    else if (args[0].e)
    {
        // If the content itself ends on sentence
        if (sentspc && is_sentence_end(args[0].s, (int)(args[0].e - args[0].s)))
            end_sentence();
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
    if (args[3].s) print_escaped(args[3].s, (int)(args[3].e - args[3].s));

    // Print the actual content within the tags
    printf("<a href=\"%.*s\">", (int)(args[0].e - args[0].s), args[0].s);
    print_escaped(args[1].s, (int)(args[1].e - args[1].s));
    printf("</a>");

    // Print the immediate suffix, if any
    if (args[2].s)
    {
        print_escaped(args[2].s, (int)(args[2].e - args[2].s));

        // If the suffix ends on sentence
        if (is_sentence_end(args[2].s, (int)(args[2].e - args[2].s)))
            end_sentence();
    }
    else if (args[1].e)
    {
        // If the content itself ends on sentence
        if (is_sentence_end(args[1].s, (int)(args[1].e - args[1].s)))
            end_sentence();
    }

    return true;
}

static bool
check_img(void)
{
    static const char *const IMG_CMD = ".IM";
    static const int IMG_CMD_LEN = strlen(IMG_CMD);

    if (len < IMG_CMD_LEN ||
        strncmp(line, IMG_CMD, IMG_CMD_LEN) != 0) return false;

    end_last_cmd();
    cmd = CMD_NONE;

    /*
     * IM commands follows this format
     *
     * .IM <uri> "image alt text"
     */

    const char *uri = line + IMG_CMD_LEN;
    uri += strspn(uri, " ");
    int uri_len = strcspn(uri, " ");
    const char *alt = uri + uri_len;
    alt += strspn(alt, " \"");
    int alt_len = strcspn(alt, "\"");

    printf(INDENT_BASE INDENT
        "<img src=\"%.*s\" alt=\"%.*s\"/>\n", uri_len, uri, alt_len, alt);

    return true;
}
