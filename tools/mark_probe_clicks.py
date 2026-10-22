"""Windows: record left mouse button observations while operating Resolume.

No input is generated or blocked. Polling is approximate: brief clicks can be
missed. Each click includes the preceding/current poll timestamps as bounds.
"""
import argparse
import csv
import sys
import time
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if sys.platform != 'win32':
        parser.error('Run this tool on the same Windows as Resolume.')
    import ctypes
    from ctypes import wintypes
    user32 = ctypes.WinDLL('user32', use_last_error=True)
    user32.GetAsyncKeyState.argtypes = [ctypes.c_int]
    user32.GetAsyncKeyState.restype = ctypes.c_short
    user32.GetForegroundWindow.restype = wintypes.HWND
    user32.GetWindowTextW.argtypes = [wintypes.HWND, wintypes.LPWSTR, ctypes.c_int]
    user32.GetWindowTextW.restype = ctypes.c_int
    # Exclusive creation prevents accidentally mixing tests or overwriting results.
    with args.output.open('x', newline='', encoding='utf-8-sig') as file:
        writer = csv.writer(file)
        writer.writerow(['click', 'before_wall_ms', 'after_wall_ms', 'foreground_title'])
        print('Recording left clicks. Click the test Column 5 times. Ctrl+C to finish.', flush=True)
        print('Keep other mouse operations outside the test; all left clicks are recorded.', flush=True)
        previous_down = bool(user32.GetAsyncKeyState(1) & 0x8000)
        previous_ms = time.time_ns() // 1_000_000
        count = 0
        try:
            while True:
                before_ms = previous_ms
                read_start_ms = time.time_ns() // 1_000_000
                down = bool(user32.GetAsyncKeyState(1) & 0x8000)
                after_ms = time.time_ns() // 1_000_000
                if down and not previous_down:
                    title = ctypes.create_unicode_buffer(512)
                    user32.GetWindowTextW(user32.GetForegroundWindow(), title, len(title))
                    count += 1
                    writer.writerow([count, before_ms, after_ms, title.value])
                    file.flush()
                    print(count, before_ms, after_ms, title.value, flush=True)
                previous_down, previous_ms = down, read_start_ms
                time.sleep(0.002)
        except KeyboardInterrupt:
            print('\nSaved:', args.output)


if __name__ == '__main__':
    main()
