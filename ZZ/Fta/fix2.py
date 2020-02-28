#!/usr/bin/env python
# coding=latin-1

import sys
import os
import re

#with open('CEA9601-basic-events.xml', 'r') as f:
with open('FaultTrees/baobab1-basic-events.xml', 'r') as f:
    text = f.read()


text = re.sub(r'\<define-basic-event name="([^"]*)" *\>\n', "\\1 = ", text)
text = re.sub(r'\<float value="([^"]*)" */\>\n', "\\1", text)
text = re.sub(r'\</define-basic-event *\>\n', ";", text)

for i in range(1,20): text = re.sub(r'  ', " ", text)

text = re.sub(r', \)', ")", text)
text = re.sub(r'\( ', "(", text)
text = re.sub(r' ;', ";\n", text)

print text
