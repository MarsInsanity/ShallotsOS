# TreeFrogUI website

The site is built with Jekyll and GitHub Pages. Add news as dated Markdown files in `_posts/`, for example `2026-10-21-new-device.md`:

```markdown
---
title: New device support
date: 2026-10-21
---

Write the post here. Markdown works as usual.
```

To change the site colors, edit the variables at the top of `assets/css/style.scss`.

The beginner guides in `guides/` reuse the canonical repository docs: `install.md`,
`docs/date-time.md`, the box-art and appearance sections of `theme.md`, and the
ROM-folder section of `README.md`. Before a local Jekyll build, run `python3 site/scripts/sync-guides.py`;
the Pages workflow runs the same command automatically. Generated includes are
ignored by Git and rebuilt from those source files.
