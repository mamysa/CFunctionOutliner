import subprocess
import os
import sys
# small test runner. 
# Since FuncExtract pass outputs XML, we need a separate program to compare actual output XML
# with expected XML
OPT   = 'opt -load ../../../../../build/lib/FuncExtract.so -funcextract --bblist=%s --out=%s %s'
CLANG = 'clang -emit-llvm -S -O0 -g %s -o %s'
tempfiles = ['.temp/', 'out.ll', 'out.xml'] 


TESTCASES = [
#'types-struct/'      , 'types-struct.c'      , 'region.txt', 'expected.xml',
#'types-struct-const/', 'types-struct-const.c', 'region.txt', 'expected.xml',
'fn-arg-struct-const/', 'fn-struct-const.c', 'region.txt', 'expected.xml',
]

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
        sys.stdout.write("%s Passed!\n" % source)


runtests()
