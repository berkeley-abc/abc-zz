#!/usr/bin/env python
# coding=latin-1

import sys
import os
import re

with open('CEA9601.xml', 'r') as f:
    text = f.read()


text = re.sub(r'\<define-gate name="([^"]*)" *\>\n', "\\1 = ", text)
text = re.sub(r'\</define-gate *\>\n', ";", text)
text = re.sub(r'\<basic-event name="([^"]*)" *\/>\n', "\\1, ", text)
text = re.sub(r'\<gate name="([^"]*)" *\/>\n', "\\1, ", text)
text = re.sub(r'\<or *>\n', "OR(", text)
text = re.sub(r'\<and *>\n', "AND(", text)
text = re.sub(r'\<not *>\n', "NOT(", text)
text = re.sub(r'\<atleast min="([^"]*)" *>\n', "GE_\\1(", text)

text = re.sub(r'\</[a-z]* *>\n', ")", text)

for i in range(1,20): text = re.sub(r'  ', " ", text)

text = re.sub(r', \)', ")", text)
text = re.sub(r'\( ', "(", text)
text = re.sub(r' ;', ";\n", text)

print text
