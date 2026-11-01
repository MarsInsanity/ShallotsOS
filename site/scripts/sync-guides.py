#!/usr/bin/env python3
"""Prepare website guide includes from the repository's canonical Markdown docs."""

from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
DEST = ROOT / "site/_includes/generated-guides"


def section(document, heading, next_heading=None):
    start = document.index(heading)
    end = document.find(next_heading, start + len(heading)) if next_heading else -1
    return document[start:] if end < 0 else document[start:end].rstrip() + "\n"


def website_links(markdown):
    routes = {
        "theme.md": "{{ '/guides/theming/' | relative_url }}",
        "install.md": "{{ '/guides/installation/' | relative_url }}",
        "docs/cores/ports.md": "{{ '/guides/ports/' | relative_url }}",
        "docs/date-time.md": "{{ '/guides/date-time/' | relative_url }}",
        "docs/cores/save-files.md": "{{ '/guides/save-files/' | relative_url }}",
        "froggyreader.md": "https://github.com/tzubertowski/TreeFrogUI/blob/main/docs/cores/froggyreader.md",
        "../../README.md": "{{ '/' | relative_url }}",
        "dev/usb-host-storage.md": "https://github.com/tzubertowski/TreeFrogUI/blob/main/docs/dev/usb-host-storage.md",
    }

    def replace(match):
        target, anchor = match.groups()
        if target == "README.md":
            route = {
                "#rom-folder-setup": "/guides/folders/",
                "#shortcuts": "/guides/shortcuts/",
                "#bios-files-required": "/guides/bios/",
            }.get(anchor)
            if route:
                return f"]({{{{ '{route}' | relative_url }}}}{anchor})"
        if target != "README.md" and target in routes:
            return f"]({routes[target]}{anchor or ''})"
        return f"](https://github.com/tzubertowski/TreeFrogUI/blob/main/{target}{anchor or ''})"

    return re.sub(r"\]\(((?:docs/)?[^)#]+\.md)(#[^)]*)?\)", replace, markdown)


def main():
    install = (ROOT / "install.md").read_text()
    install = re.sub(r"^# TreeFrogUI Installation Guide\s*\n", "", install, count=1)
    install = install.replace("<details>", '<details markdown="1">')
    install = re.sub(r"^> \[!(CAUTION|NOTE|IMPORTANT|TIP)\]\s*$", r"> **\1**", install, flags=re.M)
    theme = (ROOT / "theme.md").read_text()
    readme = (ROOT / "README.md").read_text()
    ports = (ROOT / "docs/cores/ports.md").read_text()
    manual_install = "\n\n".join((
        section(install, "## Step 1: Get the REQUIRED PROVIDED backup for YOUR device", "## Step 2: Copy the TreeFrogUI payload"),
        section(install, "## Step 2: Copy the TreeFrogUI payload", "## Step 3: Boot & add games"),
        section(install, "## Step 3: Boot & add games", "## Offline updates"),
    ))
    manual_install = manual_install.replace(
        "Clones (R36HD, etc.) run a different kernel/DTB - see [R36SX clones](#r36sx-clones-r36hd-etc).",
        "R36HD uses its own hardware profile with the compatible R36SX v2.6 boot/menu base.",
    )
    manual_install = manual_install.replace(
        "## Step 1: Get the REQUIRED PROVIDED backup for YOUR device",
        "## Step 1: Prepare the SD card for your handheld",
    ).replace(
        "## Step 2: Copy the TreeFrogUI payload",
        "## Step 2: Copy TreeFrogUI files to the card",
    ).replace(
        "## Step 3: Boot & add games",
        "## Step 3: Start TreeFrogUI and add games",
    )
    manual_install = re.sub(
        r"Click your device to expand\..*?substitute a different backup\.",
        "Open the section for your handheld. Download its stock backup and restore it to a freshly formatted SD card. Start from that clean backup; don't keep old system files or mix backups from different devices.",
        manual_install,
        count=1,
        flags=re.S,
    )
    bios = section(readme, "## BIOS files required", "## Troubleshooting & logs")
    bios = bios.replace("](#systems-that-need-extra-setup)", "](https://github.com/tzubertowski/TreeFrogUI#systems-that-need-extra-setup)")
    artwork = section(theme, "## Box art, game covers and thumbnails", "## Appearance settings")
    artwork = re.sub(r"### Recommended:.*?(?=### Add box art manually)", "", artwork, flags=re.S)
    guides = {
        "manual-installation.md": manual_install,
        "artwork.md": artwork,
        "theming.md": section(theme, "## Appearance settings"),
        "folders.md": section(readme, "## ROM folder setup", "## Customisation (themes"),
        "shortcuts.md": section(readme, "## Shortcuts", "## BIOS files required"),
        "bios.md": bios,
        "ports.md": re.sub(r"^# TreeFrogUI Ports Collection\s*\n", "", ports, count=1),
        "date-time.md": (ROOT / "docs/date-time.md").read_text(),
        "save-files.md": (ROOT / "docs/cores/save-files.md").read_text(),
    }
    assert "Step 1: Prepare the SD card for your handheld" in guides["manual-installation.md"]
    assert "In-Game Shortcuts" not in guides["manual-installation.md"]
    assert "## BIOS files required" not in guides["manual-installation.md"]
    assert "Add box art manually" in guides["artwork.md"]
    assert "Recommended: get box art automatically" not in guides["artwork.md"]
    assert "Appearance settings" in guides["theming.md"]
    assert "**GBA**" in guides["folders.md"]
    assert "SELECT + START" in guides["shortcuts.md"]
    assert "BIOS files required" in guides["bios.md"]
    assert "Install ports" in guides["ports.md"]
    DEST.mkdir(parents=True, exist_ok=True)
    for name, markdown in guides.items():
        (DEST / name).write_text(website_links(markdown))


if __name__ == "__main__":
    main()
