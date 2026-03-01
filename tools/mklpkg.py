#!/usr/bin/env python3
import struct
import sys
import os
import json

LPKG_MAGIC = b'LPKG'
LPKG_VERSION = 1


def make_lpkg(output_path, file_entries):
    with open(output_path, 'wb') as f:
        f.write(LPKG_MAGIC)
        f.write(struct.pack('B', LPKG_VERSION))
        f.write(struct.pack('<H', len(file_entries)))
        for install_path, local_path in file_entries:
            path_bytes = install_path.encode('utf-8')
            with open(local_path, 'rb') as df:
                data = df.read()
            f.write(struct.pack('<H', len(path_bytes)))
            f.write(path_bytes)
            f.write(struct.pack('<I', len(data)))
            f.write(data)
    print(f"Created {output_path} with {len(file_entries)} file(s)")


def make_index(packages_dir, output_path):
    packages = []
    for fname in sorted(os.listdir(packages_dir)):
        if not fname.endswith('.lpkg'):
            continue
        pkg_path = os.path.join(packages_dir, fname)
        meta_path = pkg_path.replace('.lpkg', '.json')
        if not os.path.isfile(meta_path):
            print(f"Warning: no metadata for {fname}, skipping")
            continue
        with open(meta_path, 'r') as f:
            meta = json.load(f)
        meta.setdefault('name', fname[:-5])
        packages.append(meta)
    with open(output_path, 'w') as f:
        json.dump(packages, f, indent=2)
    print(f"Created {output_path} with {len(packages)} package(s)")


def usage():
    print("Usage:")
    print("  mklpkg.py pack <output.lpkg> <install_path> <local_file> [...]")
    print("  mklpkg.py index <packages_dir> <output_index.json>")
    print()
    print("pack: Create a .lpkg package from files.")
    print("  Each file needs an install path and a local file path.")
    print("  Example: mklpkg.py pack hello.lpkg /bin/hello ./hello.bin")
    print()
    print("index: Generate index.json from a directory of .lpkg + .json files.")
    print("  Each .lpkg should have a matching .json metadata file:")
    print('  {"name":"hello","version":"1.0","description":"Hello program",')
    print('   "author":"you","license":"MIT","depends":[],"arch":"i386"}')
    sys.exit(1)


def main():
    if len(sys.argv) < 2:
        usage()

    cmd = sys.argv[1]

    if cmd == 'pack':
        if len(sys.argv) < 5 or (len(sys.argv) - 3) % 2 != 0:
            print("Error: need pairs of <install_path> <local_file>")
            usage()
        output = sys.argv[2]
        entries = []
        for i in range(3, len(sys.argv), 2):
            entries.append((sys.argv[i], sys.argv[i + 1]))
        make_lpkg(output, entries)
    elif cmd == 'index':
        if len(sys.argv) != 4:
            usage()
        make_index(sys.argv[2], sys.argv[3])
    else:
        usage()


if __name__ == '__main__':
    main()
