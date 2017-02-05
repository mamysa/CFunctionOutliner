import subprocess
import os
import sys
import xml.etree.cElementTree as ET
# small test runner. 
# Since FuncExtract pass outputs XML, we need a separate program to compare actual output XML
# with expected XML
OPT   = 'opt -load ../../../../../../build/lib/FuncExtract.so -funcextract --bblist=%s --out=%s %s -o /dev/null'
CLANG = 'clang -emit-llvm -S -O0 -g %s -o %s'
EXTRACTOR = 'python ../extractor.py --src %s --xml %s --append > %s'
CLANGCOMPILE = 'clang -O0 %s -o %s'

TESTFILES = [
    'pointer-1/', 'main.c', 'region.txt', 'vec_add_forcond_forend.xml',
    'toplevel-1/', 'main.c', 'region.txt', 'main_entry_fnend.xml',
    'toplevel-2/', 'main.c', 'region.txt', 'myfunc_entry_fnend.xml',
    'early-return-1/', 'main.c', 'region.txt', 'main_entry_return.xml',
    'const-qualifier-1/', 'main.c', 'region.txt', 'main_ifend_ifend7.xml',
    'static-1/', 'main.c', 'region.txt', 'main_ifend_ifend7.xml',
    'goto-1/', 'main.c', 'region.txt', 'main_entry_myreturnlabel.xml',
    'const-fn-pointer-1/', 'main.c', 'region.txt', 'main_ifend_ifend7.xml',
    'array-1/', 'main.c', 'region.txt', 'main_forcond_forend.xml',
    'array-2/', 'main.c', 'region.txt', 'main_forcond_forend.xml',
    'array-3/', 'main.c', 'region.txt', 'main_ifend_ifend9.xml',
    'array-4/', 'main.c', 'region.txt', 'main_ifend_ifend13.xml',
    'multiline-args/', 'main.c', 'region.txt', 'myfunction_forcond_forend.xml',
    'lit-brace-1/', 'main.c', 'region.txt', 'main_forcond_forend.xml',
]

TEMPFILES = ['.temp/', 'temp.ll', 'extracted.c', 'extracted.out', 'original.out']

def run_process(args): 
    process = subprocess.Popen(args)
    process.communicate()[0] 
    return process.returncode

# We test this by first extracting the region, and then compiling + running both original and extracted 
# region.
def runpass():
    subprocess.call(['rm', '-rf', TEMPFILES[0]]) #remove temp dir
    subprocess.call(['mkdir', TEMPFILES[0]]) ##mkdir temp directory

    for i in range(0, len(TESTFILES), 4):
        source = TESTFILES[i] + TESTFILES[i+1]
        region = TESTFILES[i] + TESTFILES[i+2]
        llvmirfile = TEMPFILES[0] + TEMPFILES[1]   # clang -emit-llvm output.
        extractsrc = TEMPFILES[0] + TEMPFILES[2]   # file we write output of extractor to.
        xmloutput  = TEMPFILES[0] + TESTFILES[i+3] # location of xml file written by llvm pass.
        

        ## gotta confirm those files exist...
        #if not os.path.isfile(source): raise Exception(source   + ' missing, exiting')
        #if not os.path.isfile(region): raise Exception(region   + ' missing, exiting')

        #compile to llvm ir
        clangcmd = CLANG % (source, llvmirfile)
        subprocess.call(clangcmd, shell=True)

        # run opt pass
        optcmd = OPT % (region, TEMPFILES[0], llvmirfile)
        subprocess.call(optcmd, shell=True)

        # run code extractor! 
        extractcmd = EXTRACTOR % (source, xmloutput, extractsrc)
        subprocess.call(extractcmd, shell=True)

        # compile both
        execextract  = TEMPFILES[0] + TEMPFILES[3]
        execoriginal = TEMPFILES[0] + TEMPFILES[4]
        extractcompile  = CLANGCOMPILE % (extractsrc, execextract) 
        originalcompile = CLANGCOMPILE % (source, execoriginal) 
        subprocess.call(extractcompile,  shell=True)
        subprocess.call(originalcompile, shell=True)

        # run both 
        extractretval  = run_process([execextract])
        originalretval = run_process([execoriginal])
        if (extractretval != originalretval):
            print('FAIL %s: Retcode mismatch - expected: %s, actual: %s' % (TESTFILES[i], originalretval, extractretval))
        else: 
            print('PASS %s: %s %s' % (TESTFILES[i], originalretval, extractretval))

runpass()
