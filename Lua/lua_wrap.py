#!/usr/bin/env python
# coding=latin-1

for line in open("lua.api"):
    t = line.strip()
    if len(t) == 0: continue
    if t[0] == '#': continue

    for i in range(1,100): t = t.replace("  ", " ")
    t = t.replace("const char", "cchar")
    t = t.replace("const void", "const_void")
    t = t.replace("const lua_Number", "const_lua_Number")
    t = t.replace("const lua_Debug", "const_lua_Debug")
    t = t.replace(" *", "*")
    t = t.replace("*", "* ")
    t = t.replace("  ", " ")
    t = t.replace("* *", "**")
    t = t.replace(";", "")
    assert(t[-1] == ')')
    t = t[:-1]

    fs = t.split()
    assert(fs[2] == '(lua_State*')

    ret = fs[0]
    name = fs[1][1:-1]
    args = fs[4:]
    assert len(args) % 2 == 0
    formals = '(' + ' '.join(args) + ')'
    if args[1::2] == []: actuals = '(L)'
    else: actuals = '(L, ' + ' '.join(args[1::2]) + ')'

    assert name[:4] == 'lua_'
    retstr = "return " if ret != "void" else ""
    print "    "+ret+" "+name[4:]+formals+" { "+retstr+name+actuals+"; }"
