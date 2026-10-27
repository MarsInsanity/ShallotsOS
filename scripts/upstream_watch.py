#!/usr/bin/env python3
"""Report new upstream commits for the projects in upstream-watch.txt.

    scripts/upstream_watch.py [--list upstream-watch.txt] [--out report.md]
                              [--all-tiers] [--now YYYY-MM-DD]

Monthly projects cover the last 35 days, quarterly ones the last 100 days
(reported in January, April, July and October, or always with --all-tiers).
A project with a pinned commit lists everything since that commit instead.
Commits are flagged when they touch MIPS code or their title reads like a
fix, crash, speed or compatibility change. Reads the GitHub API (GITHUB_TOKEN
if set); builds and changes nothing.
"""
import argparse
import datetime as dt
import json
import os
import re
import sys
import urllib.error
import urllib.request

API = "https://api.github.com"
MONTHLY_DAYS, QUARTERLY_DAYS = 35, 100
QUARTER_MONTHS = {1, 4, 7, 10}
MAX_LISTED = 40          # commits listed per project
MAX_FILE_CHECKS = 30     # per-commit file lookups per project (MIPS flag)
MIPS_PATH = re.compile(r"(^|/)(mips[^/]*|[^/]*mips)(/|\.|_|$)", re.I)
TITLE_FLAG = re.compile(
    r"\b(fix\w*|crash\w*|freez\w*|hang\w*|regress\w*|compat\w*|perf\w*|speed\w*|"
    r"faster|slow\w*|dynarec|jit|recompil\w*|mips|audio|sound|save\w*)\b", re.I)


def parse_list(path):
    projects = []
    for line in open(path, encoding="utf-8"):
        line = line.split("#", 1)[0].strip()
        if not line:
            continue
        parts = line.rsplit(None, 3)
        if len(parts) != 4 or "/" not in parts[1] or parts[2] not in ("monthly", "quarterly"):
            raise SystemExit(f"bad line in {path}: {line!r}")
        name, repo, tier, pin = parts
        projects.append({"name": name, "repo": repo, "tier": tier,
                         "pin": None if pin == "-" else pin})
    return projects


class GitHub:
    def __init__(self, token=None):
        self.token = token
        self.calls = 0

    def get(self, path):
        req = urllib.request.Request(API + path, headers={
            "Accept": "application/vnd.github+json",
            "User-Agent": "shallotsos-upstream-watch"})
        if self.token:
            req.add_header("Authorization", f"Bearer {self.token}")
        self.calls += 1
        with urllib.request.urlopen(req, timeout=30) as r:
            return json.load(r)


def touches_mips(files):
    return any(MIPS_PATH.search(f.get("filename", "")) for f in files or [])


def flags_for(title, mips):
    flags = []
    if mips:
        flags.append("MIPS")
    if TITLE_FLAG.search(title):
        flags.append("fix/speed")
    return flags


def commit_rows(gh, repo, commits):
    rows = []
    for i, c in enumerate(commits[:MAX_LISTED]):
        title = c["commit"]["message"].splitlines()[0]
        date = c["commit"]["committer"]["date"][:10]
        mips = None
        if i < MAX_FILE_CHECKS:
            try:
                mips = touches_mips(gh.get(f"/repos/{repo}/commits/{c['sha']}").get("files"))
            except urllib.error.HTTPError:
                mips = None
        rows.append({"sha": c["sha"][:7], "date": date, "title": title,
                     "url": c["html_url"], "flags": flags_for(title, bool(mips))})
    return rows


def project_report(gh, p, now):
    repo = p["repo"]
    if p["pin"]:
        cmp = gh.get(f"/repos/{repo}/compare/{p['pin']}...HEAD")
        commits = list(reversed(cmp.get("commits", [])))   # newest first
        scope = f"since the shipped commit `{p['pin']}`"
        total = cmp.get("total_commits", len(commits))
    else:
        days = MONTHLY_DAYS if p["tier"] == "monthly" else QUARTERLY_DAYS
        since = (now - dt.timedelta(days=days)).strftime("%Y-%m-%dT00:00:00Z")
        commits = gh.get(f"/repos/{repo}/commits?since={since}&per_page=100")
        scope = f"in the last {days} days"
        total = len(commits)
    return {"project": p, "scope": scope, "total": total,
            "rows": commit_rows(gh, repo, commits)}


def render(reports, errors, now, include_quarterly):
    out = [f"# Upstream watch, {now:%Y-%m-%d}", ""]
    out.append("New commits in the emulators and projects ShallotsOS builds on "
               "(`upstream-watch.txt`). **MIPS** = touches MIPS code; "
               "**fix/speed** = the title mentions a fix, crash, speed, sound, "
               "save or compatibility change. Nothing is built or shipped: "
               "test promising updates as a separate \"(latest)\" choice first.")
    if not include_quarterly:
        out.append("")
        out.append("_Quarterly projects are reported in January, April, July and October._")
    active = [r for r in reports if r["total"]]
    quiet = [r["project"]["name"] for r in reports if not r["total"]]
    out += ["", "| Project | New commits | MIPS | fix/speed |", "|---|---|---|---|"]
    for r in sorted(active, key=lambda r: (-sum("MIPS" in x["flags"] for x in r["rows"]), -r["total"])):
        mips = sum("MIPS" in x["flags"] for x in r["rows"])
        fixes = sum("fix/speed" in x["flags"] for x in r["rows"])
        out.append(f"| [{r['project']['name']}](https://github.com/{r['project']['repo']}) "
                   f"| {r['total']} | {mips or ''} | {fixes or ''} |")
    for r in active:
        p = r["project"]
        out += ["", f"## {p['name']} ({p['repo']}): {r['total']} {r['scope']}"]
        for x in r["rows"]:
            tag = f" **[{', '.join(x['flags'])}]**" if x["flags"] else ""
            out.append(f"- [`{x['sha']}`]({x['url']}) {x['date']} {x['title']}{tag}")
        if r["total"] > len(r["rows"]):
            out.append(f"- ... and {r['total'] - len(r['rows'])} more")
    if quiet:
        out += ["", "No new commits: " + ", ".join(quiet) + "."]
    if errors:
        out += ["", "Could not read: " + "; ".join(errors) + "."]
    return "\n".join(out) + "\n"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--list", default="upstream-watch.txt")
    ap.add_argument("--out", default="-")
    ap.add_argument("--all-tiers", action="store_true")
    ap.add_argument("--now")
    a = ap.parse_args()
    now = dt.datetime.strptime(a.now, "%Y-%m-%d") if a.now else dt.datetime.utcnow()
    include_quarterly = a.all_tiers or now.month in QUARTER_MONTHS
    gh = GitHub(os.environ.get("GITHUB_TOKEN"))
    reports, errors = [], []
    for p in parse_list(a.list):
        if p["tier"] == "quarterly" and not include_quarterly:
            continue
        try:
            reports.append(project_report(gh, p, now))
        except (urllib.error.URLError, KeyError, ValueError) as e:
            errors.append(f"{p['name']} ({e})")
    text = render(reports, errors, now, include_quarterly)
    if a.out == "-":
        sys.stdout.write(text)
    else:
        open(a.out, "w", encoding="utf-8").write(text)
    print(f"{len(reports)} projects, {gh.calls} API calls, {len(errors)} errors", file=sys.stderr)
    return 1 if reports == [] and errors else 0


if __name__ == "__main__":
    sys.exit(main())
