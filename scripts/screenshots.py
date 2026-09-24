#!/usr/bin/env python3
"""Regenerate the README screenshots in screenshots/ from the real binary.

Runs ./src/todo in a detached tmux session against a throwaway HOME seeded
with example data, captures each screen with its colors, renders the capture
as an SVG on an exact character grid, and screenshots that with headless
Chrome/Chromium. Your own ~/.local/share/todo database is never touched.

Requires: a built ./src/todo, tmux, and google-chrome or chromium.

Usage: scripts/screenshots.py [--out DIR] [--keep]
"""

import argparse
import html
import os
import re
import shutil
import sqlite3
import subprocess
import sys
import tempfile
import time

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TODO = os.path.join(REPO, 'src', 'todo')

# Terminal and image geometry. 150 columns keeps the Tasks footer on two rows.
COLS, ROWS = 150, 32
CW, CH, PAD = 9, 20, 8
FONT = 'DejaVu Sans Mono'
FONT_SIZE = 15

BG = (30, 30, 30)
FG = (212, 212, 212)
BORDER = (110, 110, 110)
PALETTE = {0: BG, 1: (241, 76, 76), 2: (13, 188, 121), 3: (229, 192, 123),
           4: (36, 114, 200), 5: (188, 63, 188), 6: (17, 168, 205), 7: FG}

# Box-drawing characters are drawn as lines so borders stay continuous
# across the line spacing: (up, down, left, right).
BOX = {'─': (0, 0, 1, 1), '│': (1, 1, 0, 0), '┌': (0, 1, 0, 1), '┐': (0, 1, 1, 0),
       '└': (1, 0, 0, 1), '┘': (1, 0, 1, 0), '├': (1, 1, 0, 1), '┤': (1, 1, 1, 0),
       '┬': (0, 1, 1, 1), '┴': (1, 0, 1, 1), '┼': (1, 1, 1, 1)}

# Each shot: output name and the keys that lead to it from the main view.
# The keys are undone afterwards with Esc, so shots stay independent.
SHOTS = [
    ('main-view', []),
    ('task-edit-form', ['Enter']),
    ('move-task', ['m', 'Down']),
    ('help-overlay', ['?']),
]

SESSION = 'todo-screenshots'


def seed(home, project_dir):
    """Create the database with the app itself, then add the example data."""
    run_app(home, project_dir)
    time.sleep(1)
    tmux('send-keys', '-t', SESSION, 'q')
    time.sleep(0.5)
    tmux('kill-session', '-t', SESSION, check=False)

    db = sqlite3.connect(os.path.join(home, '.local', 'share', 'todo', 'todo.db'))
    now = int(time.time())
    db.execute("INSERT INTO project (display_name, canonical_path, archived, builtin) "
               "VALUES ('atomrpc', ?, 0, 0)", (os.path.realpath(project_dir),))
    pid = db.execute("SELECT id FROM project WHERE display_name = 'atomrpc'").fetchone()[0]
    today = db.execute("SELECT id FROM project "
                       "WHERE display_name = 'Today' AND builtin = 1").fetchone()[0]

    def add(project, title, prio, order, parent=None, notes=None, done=False):
        cur = db.execute(
            "INSERT INTO task (project_id, parent_id, title, notes, status, priority, "
            "manual_order, archived, created_at, completed_at) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, 0, ?, ?)",
            (project, parent, title, notes, 1 if done else 0, prio, order, now,
             now if done else None))
        return cur.lastrowid

    fix = add(pid, 'Fix broken build', 1, 10,
              notes='CI has been red since the sqlite3 bump.\nBisect and pin the version.')
    add(pid, 'Bisect the failing commit', 3, 10, parent=fix)
    add(pid, 'Update CI cache key', 3, 20, parent=fix)
    add(pid, 'Investigate MQTT reconnect storm', 1, 20)
    add(pid, 'Write integration tests for retry logic', 2, 10)
    add(pid, 'Review event manager PR', 2, 20)
    add(pid, 'Clean up old feature branches', 3, 10)
    add(pid, 'Update README', 3, 20, done=True)
    add(today, 'Renew TLS certificate', 2, 10)
    add(today, 'Reply to hardware vendor', 3, 10)
    db.commit()
    db.close()


def tmux(*args, check=True):
    return subprocess.run(['tmux', *args], check=check, capture_output=True, text=True).stdout


def run_app(home, project_dir):
    tmux('kill-session', '-t', SESSION, check=False)
    tmux('new-session', '-d', '-s', SESSION, '-x', str(COLS), '-y', str(ROWS),
         '-c', project_dir, f'HOME={home} {TODO}')


def capture():
    time.sleep(0.5)
    return tmux('capture-pane', '-t', SESSION, '-e', '-p', '-N')


def parse(text):
    """Turn a tmux capture with SGR escapes into rows of (char, fg, reverse)."""
    grid = []
    for line in text.split('\n')[:ROWS]:
        row, fg, rev = [], None, False
        for tok in re.split(r'(\x1b\[[0-9;]*m)', line):
            if not tok.startswith('\x1b['):
                row.extend((ch, fg, rev) for ch in tok)
                continue
            codes = [int(c) if c else 0 for c in tok[2:-1].split(';')]
            i = 0
            while i < len(codes):
                c = codes[i]
                if c == 0:
                    fg, rev = None, False
                elif c == 7:
                    rev = True
                elif c == 27:
                    rev = False
                elif 30 <= c <= 37:
                    fg = PALETTE[c - 30]
                elif 90 <= c <= 97:
                    fg = PALETTE[c - 90]
                elif c == 39:
                    fg = None
                elif c == 38 and codes[i + 1:i + 2] == [5]:
                    fg = PALETTE.get(codes[i + 2] % 8)
                    i += 2
                i += 1
        grid.append(row)
    return grid


def hexc(c):
    return '#%02x%02x%02x' % c


def to_svg(text):
    width, height = PAD * 2 + COLS * CW, PAD * 2 + ROWS * CH
    out = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}">',
           f'<rect width="{width}" height="{height}" fill="{hexc(BG)}"/>',
           f'<g font-family="{FONT}" font-size="{FONT_SIZE}" xml:space="preserve">']
    lines = []
    for y, row in enumerate(parse(text)):
        y0 = PAD + y * CH
        run, run_color = [], None

        def flush():
            if run:
                xs = ' '.join(str(PAD + x * CW) for x, _ in run)
                chars = html.escape(''.join(ch for _, ch in run))
                out.append(f'<text x="{xs}" y="{y0 + FONT_SIZE}" '
                           f'fill="{hexc(run_color)}">{chars}</text>')
                run.clear()

        for x, (ch, fg, rev) in enumerate(row[:COLS]):
            fgc, bgc = fg or FG, BG
            if rev:
                fgc, bgc = bgc, fgc
            x0 = PAD + x * CW
            if bgc != BG:
                out.append(f'<rect x="{x0}" y="{y0}" width="{CW}" height="{CH}" '
                           f'fill="{hexc(bgc)}"/>')
            if ch in BOX:
                flush()
                up, down, left, right = BOX[ch]
                cx, cy = x0 + CW / 2, y0 + CH / 2
                stroke = hexc(fgc if rev else BORDER)
                segs = [s for s, on in (((x0, cy, cx, cy), left), ((cx, cy, x0 + CW, cy), right),
                                        ((cx, y0, cx, cy), up), ((cx, cy, cx, y0 + CH), down)) if on]
                for x1, y1, x2, y2 in segs:
                    lines.append(f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" '
                                 f'stroke="{stroke}" stroke-width="1"/>')
            elif ch == ' ':
                flush()
            else:
                if run and run_color != fgc:
                    flush()
                run_color = fgc
                run.append((x, ch))
        flush()
    out.append('</g>')
    out.extend(lines)
    out.append('</svg>')
    return '\n'.join(out)


def find_chrome():
    for name in ('google-chrome-stable', 'google-chrome', 'chromium', 'chromium-browser'):
        path = shutil.which(name)
        if path:
            return path
    sys.exit('screenshots.py: needs google-chrome or chromium on PATH')


def render_png(chrome, svg, work, png):
    page = os.path.join(work, 'page.html')
    with open(page, 'w', encoding='utf-8') as f:
        f.write(f'<html><body style="margin:0;background:{hexc(BG)}">{svg}</body></html>')
    subprocess.run([chrome, '--headless=new', '--disable-gpu', '--no-sandbox',
                    '--hide-scrollbars', '--force-device-scale-factor=1',
                    f'--window-size={PAD * 2 + COLS * CW},{PAD * 2 + ROWS * CH}',
                    f'--screenshot={png}', f'file://{page}'],
                   check=True, capture_output=True)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--out', default=os.path.join(REPO, 'screenshots'),
                    help='output directory (default: screenshots/)')
    ap.add_argument('--keep', action='store_true',
                    help='keep the temporary HOME and captures for inspection')
    args = ap.parse_args()

    if not os.access(TODO, os.X_OK):
        sys.exit(f'screenshots.py: build the app first ({TODO} not found)')
    if not shutil.which('tmux'):
        sys.exit('screenshots.py: needs tmux on PATH')
    chrome = find_chrome()

    work = tempfile.mkdtemp(prefix='todo-screenshots-')
    home = os.path.join(work, 'home')
    project_dir = os.path.join(home, 'work', 'atomrpc')
    os.makedirs(project_dir)
    try:
        seed(home, project_dir)
        run_app(home, project_dir)
        time.sleep(1)
        os.makedirs(args.out, exist_ok=True)
        for name, keys in SHOTS:
            for key in keys:
                tmux('send-keys', '-t', SESSION, key)
                time.sleep(0.2)
            text = capture()
            with open(os.path.join(work, name + '.ans'), 'w', encoding='utf-8') as f:
                f.write(text)
            png = os.path.join(os.path.abspath(args.out), name + '.png')
            render_png(chrome, to_svg(text), work, png)
            print(png)
            if keys:
                tmux('send-keys', '-t', SESSION, 'Escape')
                time.sleep(0.3)
    finally:
        tmux('kill-session', '-t', SESSION, check=False)
        if args.keep:
            print(f'kept {work}')
        else:
            shutil.rmtree(work, ignore_errors=True)


if __name__ == '__main__':
    main()
