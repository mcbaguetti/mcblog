/*
 * cblog - Markdown static blog generator in C
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#define MAX_POSTS  1000
#define MAX_LINE   2048
#define MAX_PATH   512
#define MAX_TITLE  256
#define MAX_DATE   64
#define MAX_SLUG   256

/* ── Data structures ───────────────────────────────── */

typedef struct {
    char title[MAX_TITLE];
    char date[MAX_DATE];
    char slug[MAX_SLUG];
    char filename[MAX_PATH];
} Post;

/* ── Helpers ───────────────────────────────────────── */

static void chomp(char *s) {
    size_t n = strlen(s);
    while (n > 0 && (s[n-1] == '\n' || s[n-1] == '\r')) s[--n] = '\0';
}

static int startswith(const char *s, const char *prefix) {
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

static void fput_escaped(char c, FILE *f) {
    if      (c == '<')  fputs("&lt;",  f);
    else if (c == '>')  fputs("&gt;",  f);
    else if (c == '&')  fputs("&amp;", f);
    else if (c == '"')  fputs("&quot;", f);
    else                fputc(c, f);
}

static void fputs_escaped(const char *s, FILE *f) {
    for (; *s; s++) fput_escaped(*s, f);
}

/*
 * Render inline Markdown: **bold**, *italic*, `code`, [text](url)
 */
static void render_inline(const char *src, FILE *f) {
    const char *p = src;
    while (*p) {
        /* code span `...` */
        if (*p == '`') {
            const char *end = strchr(p + 1, '`');
            if (end) {
                fputs("<code>", f);
                for (const char *c = p + 1; c < end; c++) fput_escaped(*c, f);
                fputs("</code>", f);
                p = end + 1;
                continue;
            }
        }
        /* bold **...** */
        if (p[0] == '*' && p[1] == '*') {
            const char *end = strstr(p + 2, "**");
            if (end) {
                fputs("<strong>", f);
                for (const char *c = p + 2; c < end; c++) fput_escaped(*c, f);
                fputs("</strong>", f);
                p = end + 2;
                continue;
            }
        }
        /* italic *...* */
        if (*p == '*') {
            const char *end = strchr(p + 1, '*');
            if (end) {
                fputs("<em>", f);
                for (const char *c = p + 1; c < end; c++) fput_escaped(*c, f);
                fputs("</em>", f);
                p = end + 1;
                continue;
            }
        }
        /* link [text](url) */
        if (*p == '[') {
            const char *text_end = strchr(p + 1, ']');
            if (text_end && text_end[1] == '(') {
                const char *url_end = strchr(text_end + 2, ')');
                if (url_end) {
                    fputs("<a href=\"", f);
                    for (const char *c = text_end + 2; c < url_end; c++) fput_escaped(*c, f);
                    fputs("\">", f);
                    for (const char *c = p + 1; c < text_end; c++) fput_escaped(*c, f);
                    fputs("</a>", f);
                    p = url_end + 1;
                    continue;
                }
            }
        }
        fput_escaped(*p, f);
        p++;
    }
}

/* render_inline_text: like render_inline but write plain text (no HTML)
 * into `out` (null-terminated). It handles code spans, bold, italic
 * and links by copying their inner text. */
static void render_inline_text(const char *src, char *out, size_t n) {
    const char *p = src;
    size_t used = 0;
    while (*p && used + 1 < n) {
        if (*p == '`') {
            const char *end = strchr(p + 1, '`');
            if (end) {
                for (const char *c = p + 1; c < end && used + 1 < n; c++) out[used++] = *c;
                p = end + 1; continue;
            }
        }
        if (p[0] == '*' && p[1] == '*') {
            const char *end = strstr(p + 2, "**");
            if (end) {
                for (const char *c = p + 2; c < end && used + 1 < n; c++) out[used++] = *c;
                p = end + 2; continue;
            }
        }
        if (*p == '*') {
            const char *end = strchr(p + 1, '*');
            if (end) {
                for (const char *c = p + 1; c < end && used + 1 < n; c++) out[used++] = *c;
                p = end + 1; continue;
            }
        }
        if (*p == '[') {
            const char *text_end = strchr(p + 1, ']');
            if (text_end && text_end[1] == '(') {
                for (const char *c = p + 1; c < text_end && used + 1 < n; c++) out[used++] = *c;
                const char *url_end = strchr(text_end + 2, ')');
                if (url_end) { p = url_end + 1; continue; }
            }
        }
        out[used++] = *p++;
    }
    out[used] = '\0';
}

/* ── CSS ───────────────────────────────────────────── */

/* write_css: write the <link> to the external stylesheet used by
 * generated pages. Pages are written into `output/`, so the href
 * points to `../style.css` in the project root. Keeping CSS in a
 * single file avoids embedding large inline style blocks in every
 * generated HTML file. */
static void write_css(FILE *f) {
    fprintf(f, "  <link rel=\"stylesheet\" href=\"/assets/style.css\">\n");
}

/* ── Page shell ────────────────────────────────────── */

/* write_page_open: emit the HTML page header and site header.
 * - `title` should be the page title (e.g. "Post Title — cblog").
 * - `home` is the link used for the site title (typically "index.html"
 *   for post pages, or "#" for the index page).
 * This function calls write_css() to include the stylesheet link. */
static void write_page_open(FILE *f, const char *title, const char *home) {
    fprintf(f,
        "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n"
        "  <meta charset=\"UTF-8\">\n"
        "  <meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n"
        "  <title>%s</title>\n", title);
    write_css(f);
    fprintf(f,
        "</head>\n<body>\n<div class=\"container\">\n"
        "  <header class=\"site-header\">\n"
        "    <div style=\"display:flex;align-items:center;gap:.5rem;justify-content:space-between\">\n"
        "      <div class=\"site-title\"><a href=\"%s\"><span><</span>mcbaguetti</a><span>></span></div>\n"
        "      <div>\n"
        "        <button id=\"theme-toggle\" aria-label=\"Toggle theme\">Light</button>\n"
        "      </div>\n"
        "    </div>\n"
        "  </header>\n", home);
}

static void write_page_close(FILE *f) {
    fprintf(f,
        "  <footer><a href=\"https://x.com/mcbaguetti\">X</a>&mdash;<a href=\"rss.xml\">RSS</a></footer>\n"
        "</div>\n"
           "<script src=\"/assets/oneko.js\" data-cat=\"/assets/black.gif\"></script>\n"
        "<script>\n"
        "(function(){\n"
        "  var btn = document.getElementById('theme-toggle');\n"
        "  function applyTheme(t){\n"
        "    if(t==='light') document.documentElement.setAttribute('data-theme','light');\n"
        "    else document.documentElement.removeAttribute('data-theme');\n"
        "    if(btn) btn.textContent = (t==='light') ? 'Dark' : 'Light';\n"
        "  }\n"
        "  try{ var stored = localStorage.getItem('theme'); }catch(e){var stored=null;}\n"
        "  applyTheme(stored==='light' ? 'light' : 'dark');\n"
        "  if(!btn) return;\n"
        "  btn.addEventListener('click', function(){\n"
        "    var cur = document.documentElement.getAttribute('data-theme')==='light' ? 'light' : 'dark';\n"
        "    var next = cur==='light' ? 'dark' : 'light';\n"
        "    applyTheme(next);\n"
        "    try{ localStorage.setItem('theme', next); }catch(e){}\n"
        "  });\n"
        "})();\n"
        "</script>\n"
        "</body>\n</html>\n");
}

/* ── Markdown body renderer ────────────────────────── */

static void render_markdown_body(FILE *src, FILE *f) {
    char line[MAX_LINE];
    int in_para  = 0;
    int in_ul    = 0;
    int in_fence = 0;

    while (fgets(line, sizeof(line), src)) {
        chomp(line);

        /* fenced code block */
        if (startswith(line, "```")) {
            if (!in_fence) {
                if (in_para) { fprintf(f, "</p>\n"); in_para = 0; }
                if (in_ul)   { fprintf(f, "    </ul>\n"); in_ul = 0; }
                fprintf(f, "    <pre><code>");
                in_fence = 1;
            } else {
                fprintf(f, "</code></pre>\n");
                in_fence = 0;
            }
            continue;
        }
        if (in_fence) { fputs_escaped(line, f); fputc('\n', f); continue; }

        /* headings */
        if (startswith(line, "### ")) {
            if (in_para) { fprintf(f, "</p>\n"); in_para = 0; }
            if (in_ul)   { fprintf(f, "    </ul>\n"); in_ul = 0; }
            fprintf(f, "    <h3>"); render_inline(line + 4, f); fprintf(f, "</h3>\n");
            continue;
        }
        if (startswith(line, "## ")) {
            if (in_para) { fprintf(f, "</p>\n"); in_para = 0; }
            if (in_ul)   { fprintf(f, "    </ul>\n"); in_ul = 0; }
            fprintf(f, "    <h2>"); render_inline(line + 3, f); fprintf(f, "</h2>\n");
            continue;
        }
        if (startswith(line, "# ")) {
            if (in_para) { fprintf(f, "</p>\n"); in_para = 0; }
            if (in_ul)   { fprintf(f, "    </ul>\n"); in_ul = 0; }
            fprintf(f, "    <h1>"); render_inline(line + 2, f); fprintf(f, "</h1>\n");
            continue;
        }

        /* list item */
        if (startswith(line, "- ") || startswith(line, "* ")) {
            if (in_para) { fprintf(f, "</p>\n"); in_para = 0; }
            if (!in_ul)  { fprintf(f, "    <ul>\n"); in_ul = 1; }
            fprintf(f, "      <li>"); render_inline(line + 2, f); fprintf(f, "</li>\n");
            continue;
        }

        /* blank line */
        if (strlen(line) == 0) {
            if (in_para) { fprintf(f, "</p>\n"); in_para = 0; }
            if (in_ul)   { fprintf(f, "    </ul>\n"); in_ul = 0; }
            continue;
        }

        /* paragraph text */
        if (in_ul) { fprintf(f, "    </ul>\n"); in_ul = 0; }
        if (!in_para) { fprintf(f, "    <p>"); in_para = 1; }
        else           fprintf(f, " ");
        render_inline(line, f);
    }

    if (in_para) fprintf(f, "</p>\n");
    if (in_ul)   fprintf(f, "    </ul>\n");
    if (in_fence) fprintf(f, "</code></pre>\n");
}

/* ── Frontmatter parser ────────────────────────────── */

static int parse_frontmatter(const char *filepath, char *title, char *date) {
    FILE *f = fopen(filepath, "r");
    if (!f) return 0;

    char line[MAX_LINE];
    title[0] = date[0] = '\0';

    if (!fgets(line, sizeof(line), f)) { fclose(f); return 0; }
    chomp(line);
    if (strcmp(line, "---") != 0) { fclose(f); return 0; }

    while (fgets(line, sizeof(line), f)) {
        chomp(line);
        if (strcmp(line, "---") == 0) break;
        if (startswith(line, "title:")) {
            const char *v = line + 6; while (*v == ' ') v++;
            strncpy(title, v, MAX_TITLE - 1);
        } else if (startswith(line, "date:")) {
            const char *v = line + 5; while (*v == ' ') v++;
            strncpy(date, v, MAX_DATE - 1);
        }
    }
    fclose(f);
    return (title[0] != '\0');
}


/* ── Post & index generators ───────────────────────── */

static void generate_post(Post *p, const char *outdir) {
    FILE *src = fopen(p->filename, "r");
    if (!src) { perror(p->filename); return; }

    char outpath[MAX_PATH];
    snprintf(outpath, sizeof(outpath), "%s/%s.html", outdir, p->slug);
    FILE *f = fopen(outpath, "w");
    if (!f) { perror(outpath); fclose(src); return; }

    /* Skip frontmatter */
    char line[MAX_LINE]; int fm = 0;
    while (fgets(line, sizeof(line), src)) {
        chomp(line);
        if (strcmp(line, "---") == 0 && ++fm == 2) break;
    }

    char page_title[MAX_TITLE + 16];
    snprintf(page_title, sizeof(page_title), "%s &mdash; mcbg", p->title);
    write_page_open(f, page_title, "index.html");

    fprintf(f,
        "  <article>\n"
        "    <div class=\"post-header\">\n"
        "      <h1 class=\"post-title\">%s</h1>\n"
        "      <div class=\"post-date\">%s</div>\n"
        "    </div>\n"
        "    <div class=\"post-body\">\n",
        p->title, p->date);

    render_markdown_body(src, f);

    fprintf(f,
        "    </div>\n  </article>\n"
        "  <a class=\"back\" href=\"index.html\">&larr; back to index</a>\n");

    /* no embedded canvas or main.js by default */

    write_page_close(f);
    fclose(src); fclose(f);
    printf("  [post]   %s\n", outpath);
}

/* generate_index: write `outdir/index.html` listing the posts.
 * Ordering: this function writes posts in the order they appear in the
 * `posts[]` array. That array is populated by `load_posts()` which
 * appends posts in the order returned by the filesystem via `readdir()`.
 * No sorting is performed here — posts appear in filesystem order.
 * To change ordering (by date, title, or reverse), sort the `posts`
 * array before calling generate_index (e.g., with qsort()). */
static void generate_index(Post posts[], int count, const char *outdir) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/index.html", outdir);
    FILE *f = fopen(path, "w");
    if (!f) { perror(path); return; }

    write_page_open(f, "<mcbaguetti>", "#");
    /* Search UI */
    fprintf(f,
        "  <div class=\"search\">\n"
        "    <input id=\"search\" placeholder=\"Search posts...\">\n"
        "    <button id=\"clear\">Clear</button>\n"
        "  </div>\n");
    fprintf(f, "  <ul class=\"post-list\" id=\"post-list\">\n");
    for (int i = 0; i < count; i++) {
        /* Read post source to collect plain text for client-side search
         * and extract the first paragraph as an excerpt. */
        char content[4096]; content[0] = '\0';
        char para_raw[1024]; para_raw[0] = '\0';
        FILE *src = fopen(posts[i].filename, "r");
        if (src) {
            char line[MAX_LINE]; int fm = 0; int in_para = 0;
            while (fgets(line, sizeof(line), src)) {
                chomp(line);
                if (strcmp(line, "---") == 0 && ++fm == 2) break;
            }
            while (fgets(line, sizeof(line), src)) {
                chomp(line);
                /* build searchable content (strip heading markers) */
                if (line[0] == '#') {
                    const char *p = line; while (*p == '#') p++; while (*p == ' ') p++;
                    strncat(content, p, sizeof(content) - strlen(content) - 2);
                } else {
                    strncat(content, line, sizeof(content) - strlen(content) - 2);
                }
                strncat(content, " ", sizeof(content) - strlen(content) - 1);

                /* capture first paragraph (raw markdown) */
                if (!in_para) {
                    if (strlen(line) > 0) {
                        strncat(para_raw, line, sizeof(para_raw) - strlen(para_raw) - 2);
                        strncat(para_raw, " ", sizeof(para_raw) - strlen(para_raw) - 1);
                        in_para = 1;
                    }
                } else {
                    if (strlen(line) == 0) { /* paragraph ended */ }
                    else if (strlen(para_raw) < sizeof(para_raw) - 100) {
                        strncat(para_raw, line, sizeof(para_raw) - strlen(para_raw) - 2);
                        strncat(para_raw, " ", sizeof(para_raw) - strlen(para_raw) - 1);
                    }
                }

                if (strlen(content) > sizeof(content) - 100) break; /* limit size */
            }
            fclose(src);
        }

        /* render plain-text excerpt from first paragraph */
        char excerpt[512]; excerpt[0] = '\0';
        if (para_raw[0]) {
            render_inline_text(para_raw, excerpt, sizeof(excerpt));
            if (strlen(excerpt) > 220) excerpt[220] = '\0';
        }

        fprintf(f, "    <li data-content=");
        fputs_escaped(content, f);
        fprintf(f, "\">\n");
        /* Header: title (left) and date (right) on the same row */
        fprintf(f, "      <div class=\"item-header\">\n");
        fprintf(f, "        <a class=\"item-title\" href=\"%s.html\">%s</a>\n", posts[i].slug, posts[i].title);
        fprintf(f, "        <div class=\"item-meta-header\">\n");
        fprintf(f, "          <span class=\"date\">%s</span>\n", posts[i].date);
        fprintf(f, "        </div>\n");
        fprintf(f, "      </div>\n");
        /* Body: excerpt only */
        fprintf(f, "      <div class=\"item-body\">\n");
        fprintf(f, "        <div class=\"excerpt\">\n");
        fputs_escaped(excerpt, f);
        fprintf(f, "</div>\n");
        /* right-aligned read link; margin-left:auto pushes it to the
         * right edge so its rightmost point matches the header date */
        fprintf(f, "        <a class=\"readmore\" href=\"%s.html\">[full_post]</a>\n", posts[i].slug);
        fprintf(f, "      </div>\n");
        fprintf(f, "    </li>\n");
    }
    fprintf(f, "  </ul>\n");
    /* Client-side search script: filters list items by title, date, or
     * extracted content. Runs in the browser; no server required. */
    fprintf(f,
        "<script>\n"
        "(function(){\n"
        "  var input = document.getElementById('search');\n"
        "  var clear = document.getElementById('clear');\n"
        "  var list = document.getElementById('post-list');\n"
        "  if(!input||!list) return;\n"
        "  var items = Array.prototype.slice.call(list.querySelectorAll('li'));\n"
        "  function norm(s){return (s||'').toLowerCase();}\n"
        "  input.addEventListener('input', function(){\n"
        "    var q = norm(this.value.trim());\n"
        "    items.forEach(function(li){\n"
        "      var title = norm(li.querySelector('a').textContent);\n"
        "      var date = norm((li.querySelector('.date')||{}).textContent);\n"
        "      var content = norm(li.getAttribute('data-content'));\n"
        "      var ok = q === '' || title.indexOf(q) !== -1 || date.indexOf(q) !== -1 || content.indexOf(q) !== -1;\n"
        "      li.style.display = ok ? '' : 'none';\n"
        "    });\n"
        "  });\n"
        "  clear.addEventListener('click', function(){ input.value=''; input.dispatchEvent(new Event('input')); });\n"
        "})();\n"
        "</script>\n");
    write_page_close(f);
    fclose(f);
    printf("  [index]  %s\n", path);
}

/* format_rfc822: convert YYYY-MM-DD -> RFC 822 date string
 * Example: "2026-03-21" -> "Sun, 21 Mar 2026 00:00:00 +0000" */
static void format_rfc822(const char *ymd, char *out, size_t n) {
    int y = 0, m = 0, d = 0;
    if (sscanf(ymd, "%4d-%2d-%2d", &y, &m, &d) != 3) {
        snprintf(out, n, "%s", ymd ? ymd : "");
        return;
    }
    /* Sakamoto's algorithm for day of week */
    static const int t[] = {0,3,2,5,0,3,5,1,4,6,2,4};
    int yy = y;
    if (m < 3) yy -= 1;
    int w = (yy + yy/4 - yy/100 + yy/400 + t[m-1] + d) % 7; /* 0 = Sunday */
    static const char *wd[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    static const char *mn[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
    snprintf(out, n, "%s, %02d %s %04d 00:00:00 +0000", wd[w], d, mn[m-1], y);
}

/* generate_rss: write a simple RSS 2.0 feed at outdir/rss.xml.
 * Links are relative ("slug.html") when no absolute base URL is provided.
 */
static void generate_rss(Post posts[], int count, const char *outdir, const char *base_url) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/rss.xml", outdir);
    FILE *f = fopen(path, "w");
    if (!f) { perror(path); return; }

    /* Channel metadata */
    fprintf(f, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n");
    fprintf(f, "<rss version=\"2.0\">\n<channel>\n");
    fprintf(f, "  <title>mcbaguetti</title>\n");
    if (base_url && base_url[0]) fprintf(f, "  <link>%s</link>\n", base_url);
    else fprintf(f, "  <link></link>\n");
    fprintf(f, "  <description>mcbaguetti posts</description>\n");

    for (int i = 0; i < count; i++) {
        char link[MAX_PATH];
        if (base_url && base_url[0]) snprintf(link, sizeof(link), "%s/%s.html", base_url, posts[i].slug);
        else snprintf(link, sizeof(link), "%s.html", posts[i].slug);

        char pub[64]; format_rfc822(posts[i].date, pub, sizeof(pub));

        fprintf(f, "  <item>\n");
        fprintf(f, "    <title>"); fputs_escaped(posts[i].title, f); fprintf(f, "</title>\n");
        fprintf(f, "    <link>%s</link>\n", link);
        fprintf(f, "    <guid isPermaLink=\"false\">%s</guid>\n", link);
        if (pub[0]) fprintf(f, "    <pubDate>%s</pubDate>\n", pub);
        fprintf(f, "  </item>\n");
    }

    fprintf(f, "</channel>\n</rss>\n");
    fclose(f);
    printf("  [rss]    %s\n", path);
}

/* Comparator used to sort posts by date in descending order (newest first).
 * Dates are expected in ISO format `YYYY-MM-DD`, so simple string
 * comparison gives correct chronological ordering. Empty or missing
 * dates are treated as older than any real date. */
static int cmp_post_date_desc(const void *aa, const void *bb) {
    const Post *a = aa;
    const Post *b = bb;
    if (a->date[0] == '\0' && b->date[0] == '\0') return 0;
    if (a->date[0] == '\0') return 1; /* a is older */
    if (b->date[0] == '\0') return -1; /* b is older */
    int r = strcmp(b->date, a->date); /* b vs a -> descending */
    if (r != 0) return r;
    return strcmp(a->slug, b->slug); /* tie-break by slug */
}

/* ── Post loader ───────────────────────────────────── */

static int load_posts(const char *postsdir, Post posts[], int maxposts) {
    DIR *d = opendir(postsdir);
    if (!d) { perror(postsdir); return 0; }

    int count = 0;
    struct dirent *entry;

    while ((entry = readdir(d)) != NULL && count < maxposts) {
        char *name = entry->d_name;
        size_t len  = strlen(name);
        if (len < 4 || strcmp(name + len - 3, ".md") != 0) continue;

        Post *p = &posts[count];
        strncpy(p->slug, name, len - 3);
        p->slug[len - 3] = '\0';
        snprintf(p->filename, sizeof(p->filename), "%s/%s", postsdir, name);

        if (!parse_frontmatter(p->filename, p->title, p->date)) {
            fprintf(stderr, "  [skip]   %s (missing frontmatter)\n", p->filename);
            continue;
        }
        count++;
    }
    closedir(d);
    return count;
}

/* ── main ──────────────────────────────────────────── */

int main(void) {
    const char *postsdir = "posts";
    const char *outdir   = "output";

    Post posts[MAX_POSTS];
    int count = load_posts(postsdir, posts, MAX_POSTS);

    if (count == 0) {
        fprintf(stderr, "No .md posts found in '%s/'\n", postsdir);
        return 1;
    }

    printf("mcbaguetti: found %d post(s)\n", count);
    /* Sort posts by date (newest first) so the index shows recent posts on top. */
    qsort(posts, count, sizeof(Post), cmp_post_date_desc);
    generate_index(posts, count, outdir);
    for (int i = 0; i < count; i++)
        generate_post(&posts[i], outdir);
    /* Generate RSS feed in output/ (relative links used when base_url is empty). */
    generate_rss(posts, count, outdir, "");

    printf("Done. Open output/index.html in your browser.\n");
    return 0;
}
