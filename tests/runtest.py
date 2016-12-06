import subprocess
import os
import sys
import xml.etree.cElementTree as ET
# small test runner. 
# Since FuncExtract pass outputs XML, we need a separate program to compare actual output XML
# with expected XML
#OPT   = 'opt -load ../../../../../build/lib/FuncExtract.so -funcextract --bblist=%s --out=%s %s -o /dev/null'
OPT   = 'opt -load ../../../../../build/lib/FuncExtract.so -funcextract --bblist=%s --out=%s %s -o /dev/null &> /dev/null'
CLANG = 'clang -emit-llvm -S -O0 -g %s -o %s'
tempfiles = ['.temp/', 'out.ll', 'out.xml'] 


TESTCASES = [
'fn-pointer-1/',     'main.c', 'region.txt', 'expected.xml', 
'fn-pointer-2/',     'main.c', 'region.txt', 'expected.xml',
'fn-pointer-3/',     'main.c', 'region.txt', 'expected.xml', 
'fn-pointer-const/', 'main.c', 'region.txt', 'expected.xml',
'fn-pointer-typedef/', 'main.c', 'region.txt', 'expected.xml',
'type-primitive-1/', 'main.c', 'region.txt', 'expected.xml',
'type-primitive-const-1/', 'main.c', 'region.txt', 'expected.xml',
'type-primitive-const-2/', 'main.c', 'region.txt', 'expected.xml',


#'type-struct-local/'      , 'main.c'      , 'region.txt', 'expected.xml',
#'type-struct-global/'      , 'main.c'      , 'region.txt', 'expected.xml',
#'type-struct-anonymous/'      , 'main.c'      , 'region.txt', 'expected.xml', 'EXPECTED FAIL',
#'type-enum-global/'      , 'main.c'      , 'region.txt', 'expected.xml', '', 
#'fn-pointer-3/', 'main.c', 'region.txt', 'expected.xml',
#'fn-pointer-const/', 'main.c', 'region.txt', 'expected.xml',
#'fn-pointer-typedef/', 'main.c', 'region.txt', 'expected.xml',
#'type-ptr-void/', 'main.c', 'region.txt', 'expected.xml',


#'types-struct-const/', 'main.c', 'region.txt', 'expected.xml',
#'fn-arg-struct-const/', 'main.c', 'region.txt', 'expected.xml',
#'types-array/', 'main.c', 'region.txt', 'expected.xml',
#'types-array-const/', 'main.c', 'region.txt', 'expected.xml',
#'types-array-const/', 'main.c', 'region.txt', 'expected.xml',
#'type-primitive-const-1/', 'main.c', 'region.txt', 'expected.xml', ''
#'fn-arg-const-basic/', 'main.c', 'region.txt', 'expected.xml',
]

## reads variable info from XML. expects to have both name and type. 
def xmlgetvariableinfo(filepath):
    out = []
    tree = ET.parse(filepath);
    for child in tree.getroot():
        if (child.tag == 'variable'):
            name = child.find('name').text.strip(' ').replace(' ', '')
            type = child.find('type').text.strip(' ').replace(' ', '')
            out.append((name, type))
    return out 


def cmpvars(source, expect, actual):
    ## compare lengths first...
    if len(expect) != len(actual) :
        return 'FAIL %s: expected : expected: %s, actual: %s!\n' % (source, len(expect), len(actual))
    for e in expect: 
        found = list(filter(lambda x: x[0] == e[0], actual))
        if len(found) != 1:
            return 'FAIL %s: could not find %s %s\n' % (source, e[1], e[0])
        if e[1] != found[0][1]:
            return 'FAIL %s: type mismatch expected: %s actual: %s\n' % (source, e[1], found[0][1])
    return 'PASS %s\n' % source 


def runtests():
    subprocess.call(['rm', '-rf', tempfiles[0]]) #remove temp dir
    subprocess.call(['mkdir', tempfiles[0]]) ##mkdir temp directory
    for i in range(0, len(TESTCASES), 4):
        source   = TESTCASES[i] + TESTCASES[i+1]
        region   = TESTCASES[i] + TESTCASES[i+2]
        expected = TESTCASES[i] + TESTCASES[i+3]
        outsrc   = tempfiles[0] + tempfiles[1]
        outxml   = tempfiles[0] + tempfiles[2]

        ## gotta confirm those files exist...
        if not os.path.isfile(source):   raise Exception(source   + ' missing, exiting')
        if not os.path.isfile(region):   raise Exception(region   + ' missing, exiting')
        if not os.path.isfile(expected): raise Exception(expected + ' missing, exiting')

        #compile to llvm ir
        clangcmd = CLANG % (source, outsrc)
        subprocess.call(clangcmd, shell=True)

        # run opt pass
        optcmd = OPT % (region, outxml, outsrc)
        subprocess.call(optcmd, shell=True)

        expect = xmlgetvariableinfo(expected);
        actual = xmlgetvariableinfo(outxml)

        sys.stdout.write(cmpvars(source, expect, actual))

runtests()
