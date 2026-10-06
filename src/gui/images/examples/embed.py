#!/usr/bin/env python3
# Writes a C++ file that holds the example pictures of the grid selector,
# for exampleimages.h. Run by the build: embed.py out.cpp picture.png ...
import os
import sys

out, files = sys.argv[1], sys.argv[2:]
with open(out, 'w') as f:
    f.write('/* Made by src/gui/images/examples/embed.py; do not edit. */\n')
    f.write('#include "gui/exampleimages.h"\n\nnamespace exampleImages {\n\n')
    for i, path in enumerate(files):
        data = open(path, 'rb').read()
        f.write('static const unsigned char file%d[] = {\n' % i)
        for j in range(0, len(data), 16):
            f.write('  ' + ', '.join('%d' % b for b in data[j:j + 16]) + ',\n')
        f.write('};\n\n')
    f.write('const file_c files[] = {\n')
    for i, path in enumerate(files):
        name = os.path.splitext(os.path.basename(path))[0]
        f.write('  {"%s", file%d, sizeof(file%d)},\n' % (name, i, i))
    f.write('};\n\nconst unsigned int count = %d;\n\n} // namespace exampleImages\n' % len(files))
