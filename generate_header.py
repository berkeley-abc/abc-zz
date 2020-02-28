import os
import sys
import re


ifndef_re = re.compile(r'^\s*#\s*ifndef\s+(\S+)$')


def find_guard(fname):

    with open(fname, 'r') as f:
        for line in f.readlines():
            m = ifndef_re.match(line)
            if m:
                return m.group(1)

    return None


zz_dir = sys.argv[1]
fname = sys.argv[2]
headers = sys.argv[3:]


TEMPLATE="""\
#ifndef {guard}
#include \"{header}\"
#endif
"""

def guards():

    for hh in headers:
        guard = find_guard(hh)
        hh = os.path.relpath(hh, zz_dir)
        if guard:
            yield TEMPLATE.format(guard=guard, header=hh)

with open(fname, 'w') as fout:
    fout.write("\n".join(guards()))
