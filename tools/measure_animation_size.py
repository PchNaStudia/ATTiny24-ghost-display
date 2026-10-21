"""Build every animation-family combination against a fixed no-animation schedule.
The source configuration is never edited. Reports are saved in .pio/effect-size.
"""
from pathlib import Path
import json
import re
import shutil
import subprocess

ROOT = Path(__file__).resolve().parent.parent
OUTPUT = ROOT / '.pio' / 'effect-size'
# Include every named direction/variant, both layouts and both strips.
FAMILIES = {
    'Wipe': ['WipeIn', 'WipeOut', 'WipeInReverse', 'WipeOutReverse'],
    'Walk': ['WalkIn', 'WalkOut', 'WalkInReverse', 'WalkOutReverse'],
    'Typewriter': ['Typewriter', 'Backspace', 'ForwardDelete'],
    'Blink': ['Blink'],
    'OneByOne': ['OneByOne'],
}


def main():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    for folder in ('include', 'src'):
        shutil.copytree(ROOT / folder, OUTPUT / folder, dirs_exist_ok=True)
    shutil.copy2(ROOT / 'platformio.ini', OUTPUT / 'platformio.ini')
    config = (ROOT / 'include' / 'animation_config.hpp').read_text()
    prefix = config[:config.index('constexpr AnimationEntry animation[]')]
    platformio = Path.home() / '.platformio' / 'penv' / 'Scripts' / 'platformio.exe'
    families = list(FAMILIES)
    results = []
    for mask in range(32):
        entries = []
        for bit, effects in enumerate(FAMILIES.values()):
            for effect in effects:
                for frame in range(1):
                    frame = len(entries) % 2
                    strip = 'Second' if frame else 'First'
                    selected = effect if mask & (1 << bit) else 'None'
                    entries.append(f'    virtualFrame<{frame}>(255, Animation::{selected}, Strip::{strip}),')
        schedule = 'constexpr AnimationEntry animation[] HAL_STORAGE = {\n' + '\n'.join(entries) + '\n};\n'
        (OUTPUT / 'include' / 'animation_config.hpp').write_text(prefix + schedule)
        process = subprocess.run([str(platformio), 'run', '-d', str(OUTPUT), '-e', 'attiny24'],
                                 capture_output=True, text=True)
        (OUTPUT / f'build-{mask:02d}.log').write_text(process.stdout + process.stderr)
        flash = re.search(r'used (\d+) bytes from 2048', process.stdout)
        ram = re.search(r'used (\d+) bytes from 128', process.stdout)
        if process.returncode or not flash or not ram:
            raise RuntimeError(f'Build {mask} failed; see {OUTPUT / f"build-{mask:02d}.log"}')
        names = [name for bit, name in enumerate(families) if mask & (1 << bit)]
        record = dict(mask=mask, families=names, flash=int(flash[1]), ram=int(ram[1]))
        record['delta'] = record['flash'] - (results[0]['flash'] if results else record['flash'])
        results.append(record)
        print(f'{mask:02d} {" + ".join(names) or "None"}: {record["flash"]} bytes ({record["delta"]:+d})', flush=True)
    columns = [('None', 0), ('Blink', 8), ('OneByOne', 16), ('Blink + OneByOne', 24)]
    lines = [f'No-animation baseline: {results[0]["flash"]} bytes flash; {results[0]["ram"]} bytes static RAM.',
             'Fixed 13-entry schedule, same frames/durations/strips; every named variant included.',
             '', '| Wipe / Walk / Typewriter | ' + ' | '.join(name for name, _ in columns) + ' |',
             '|---|' + '---:|' * 4]
    for row in range(8):
        label = ' + '.join(families[bit] for bit in range(3) if row & (1 << bit)) or 'None'
        lines.append('| ' + label + ' | ' + ' | '.join(f'{results[row | col]["delta"]:+d}' for _, col in columns) + ' |')
    report = '\n'.join(lines) + '\n'
    (OUTPUT / 'sizes.json').write_text(json.dumps(results, indent=2))
    (OUTPUT / 'grid.md').write_text(report)
    print('\n' + report)


if __name__ == '__main__':
    main()
