# cblog

A minimal static blog generator written in C. No dependencies, no build system, no runtime — just a single C file that reads Markdown posts and outputs HTML.

---

## Requirements

- A C compiler (`gcc` or `clang`)
- A POSIX-compatible system (Linux (or WSL), macOS)

---

## Getting Started

### 1. Compile

```bash
gcc -o cblog blog.c
```

### 2. Create the folder structure

```
cblog/
├── blog.c
├── cblog          ← compiled binary
├── posts/         ← your .md files go here
└── output/        ← generated HTML goes here
```

```bash
mkdir posts output
```

### 3. Write a post

Create a `.md` file inside `posts/`. Every post **must** start with a frontmatter block:

```markdown
---
title: My First Post
date:  2026-03-21
---

Your post content goes here. Write in **Markdown**.
```

### 4. Generate the site

```bash
./cblog
```

This will generate `output/index.html` and one HTML file per post. Open `output/index.html` in your browser.

---

## Markdown Support

| Syntax | Result |
|---|---|
| `# Heading` | `<h1>` |
| `## Heading` | `<h2>` |
| `### Heading` | `<h3>` |
| `**bold**` | **bold** |
| `*italic*` | *italic* |
| `` `code` `` | inline code |
| `[text](url)` | hyperlink |
| `- item` | unordered list item |
| ` ``` ` ... ` ``` ` | fenced code block |
| blank line | paragraph break |

---

## Post Format

```markdown
---
title: The Title of Your Post
date:  YYYY-MM-DD
---

First paragraph of your post.

## A Section Heading

More content with **bold**, *italic*, and `inline code`.

- List item one
- List item two

A [link to somewhere](https://example.com).

```bash
# A fenced code block
gcc -o cblog blog.c
```
```

> **Rules:**
> - The `---` delimiters are required.
> - `title:` and `date:` are the only supported frontmatter fields.
> - The filename (without `.md`) becomes the URL slug — e.g. `my-post.md` → `my-post.html`.

---

## Project Structure

```
posts/
    my-post.md        ← source files (you write these)

output/
    index.html        ← blog index (generated)
    my-post.html      ← post pages (generated)
```

---

## Rebuilding

Every time you add or edit a post, just run:

```bash
./cblog
```

All files in `output/` are overwritten on each run.

---

## Deploying

The `output/` folder is a self-contained static site. You can host it anywhere that serves HTML files:

- **GitHub Pages** — push `output/` to a `gh-pages` branch
- **Netlify / Vercel** — drag and drop the `output/` folder
- **Any web server** — copy `output/` to your server's web root (e.g. `/var/www/html/`)

---

## Notes

- Posts are listed in the order the filesystem returns them (no automatic date sorting).
- `dirent.h` is used for directory scanning — this will **not** compile natively on Windows. Use WSL or MinGW if on Windows.
- There is no configuration file. Edit `blog.c` directly to change the site title, colors, or CSS.
