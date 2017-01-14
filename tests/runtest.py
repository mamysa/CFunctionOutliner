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
tempfiles = ['.temp/', 'out.ll'] 


TESTFILES = [
    'general-1/', 'main.c', 'regions.txt',
    'general-2/', 'main.c', 'regions.txt',
    'type-basic-const/', 'main.c', 'regions.txt',
    'type-struct/',      'main.c', 'regions.txt',
    'type-pointers/',    'main.c', 'regions.txt',
    'type-array/',       'main.c', 'regions.txt',
    'type-fn-pointers/', 'main.c', 'regions.txt',
    'bad-cases/',        'main.c', 'regions.txt',
]

TESTCASES = {

    'general-1/': [ 'test1_forcond_forend.xml'  , '',
                    'test2_entry_ifend.xml'     , '',
                    'test3_ifend_ifend9.xml'    , '',
                    'test4_forcond_forend.xml'  , '',
                    'test5_entry_ifend.xml'     , '',
                    'test6_forcond_forend.xml'  , '',
                    'test7_forcond_forend.xml'  , '',
                    'test8_forcond_forend.xml'  , 'EXPECTED',
                    'test9_forcond_forend.xml'  , '', 
                    'test10_entry_swepilog.xml' , '', 
                    'test11_forend_swepilog.xml', '', ],

    'general-2/': [ 'test1_forcond_forend.xml'  , '',
                    'test1s1_forcond_forend.xml', '',
                    'test2_forcond_forend.xml'  , '',
                    'test3_forcond_forend.xml'  , '',
                    'test2s1_forcond_forend.xml', '', ],

    'type-struct/': [ 'test1_ifend_ifend13.xml'  , '',
                      'test2_ifend_ifend14.xml'  , '',
                      'test3_ifend_ifend14.xml'  , '',
                      'test3s1_ifend_ifend22.xml', '',
                      'test3s2_ifend_ifend22.xml', '',
                      'test3s3_ifend_ifend28.xml', '',
                      'test4_ifend_ifend14.xml'  , '',
                      'test6_forcond_forend.xml' , '',
                      'test5_ifend_ifend12.xml'  , '', ],

    'type-basic-const/':     [ 'test1_forcond_forend.xml'  , ''        ,
                               'test2_ifend_ifend7.xml'    , 'EXPECTED',
                               'test3_ifend_ifend7.xml'    , ''        ,
                               'test4_ifend_ifend8.xml'    , ''        ,
                               'test5_ifend_ifend8.xml'    , 'EXPECTED',
                               'test6_forcond_forend.xml'  , ''        ,
                               'test7_forcond_forend.xml'  , ''        ,
                               'test8_forcond_forend.xml'  , 'EXPECTED',
                               'test8s1_forcond_forend.xml', 'EXPECTED', ],

    'type-pointers/':      [ 'test1_forcond_forend.xml', '',
                             'test2_forcond_forend.xml', '',
                             'test3_forcond_forend.xml', '',
                             'test4_forcond_forend.xml', '',
                             'test6_forcond_forend.xml', '',
                             'test5_forcond_forend.xml', '', ],

    'type-array/': [ 'test1_ifend_ifend13.xml'           , '',
                     'test1s1_ifend_ifend13.xml'         , '',
                     'test1s2_arrayinitend12_ifend33.xml', '',
                     'test2_ifend_ifend14.xml'           , '',
                     'test3_ifend_ifend14.xml'           , '',
                     'test4_ifend_ifend23.xml'           , '', ],


    'type-fn-pointers/':       [ 'test1_forcond_forend.xml', '',
                                 'test2_forcond_forend.xml', '',
                                 'test3_forcond_forend.xml', '',
                                 'test4_forcond_forend.xml', '',
                                 'test5_forcond_forend.xml', '', ],

    'bad-cases/': [ 'test1_forcond_forend.xml', 'EXPECTED',
                    'test2_forcond_forend.xml', 'EXPECTED', ]
}

class VariableInfo:
    def __init__(self):
        self.name = ''
        self.type = ''
        self.isfunptr = False
        self.isstatic = False
        self.isconstq = False

    def compare_with_error(self, other):
        if self.name != other.name: return 'Name mismatch: %s %s' % (self.name, other.name)
        if self.type != other.type: return 'Type mismatch: %s %s' % (self.type, other.type)
        if self.isfunptr != other.isfunptr: return 'isfunptr mismatch: %s %s' % (self.isfunptr, other.isfunptr)
        if self.isstatic != other.isstatic: return 'isstatic mismatch: %s %s' % (self.isstatic, other.isstatic)
        if self.isconstq != other.isconstq: return 'isconstq mismatch: %s %s' % (self.isconstq, other.isconstq)
        return '' 

## reads variable info from XML. expects to have both name and type. 
def xmlgetvariableinfo(filepath):
    out = []
    tree = ET.parse(filepath);
    for child in tree.getroot():
        if (child.tag == 'variable'):
            var = VariableInfo()
            var.name = child.find('name').text.strip(' ').replace(' ', '')
            var.type = child.find('type').text.strip(' ').replace(' ', '')
            if child.find('isfunptr') != None: var.isfunptr = bool(int(child.find('isfunptr').text))
            if child.find('isstatic') != None: var.isstatic = bool(int(child.find('isstatic').text))
            if child.find('isconstq') != None: var.isconstq = bool(int(child.find('isconstq').text))
            out.append(var)
    return out 


def cmpvars(source, expect, actual):
    ## compare lengths first...
    if len(expect) != len(actual) :
        return 'FAIL %s: expected: %s, actual: %s!\n' % (source, len(expect), len(actual))
    for e in expect: 
        found = list(filter(lambda x: x.name == e.name, actual))
        if len(found) != 1:
            return 'FAIL %s: could not find %s %s\n' % (source, e.type, e.name)
        cmpstatus = e.compare_with_error(found[0])
        if cmpstatus != '': 
            return 'FAIL %s: %s\n' % (source, cmpstatus)
    return 'PASS %s\n' % source 


def runtests():
    subprocess.call(['rm', '-rf', tempfiles[0]]) #remove temp dir
    subprocess.call(['mkdir', tempfiles[0]]) ##mkdir temp directory
    for i in range(0, len(TESTFILES), 3):
        source = TESTFILES[i] + TESTFILES[i+1]
        region = TESTFILES[i] + TESTFILES[i+2]
        outsrc = tempfiles[0] + tempfiles[1]
        outdir = tempfiles[0]

        ## gotta confirm those files exist...
        if not os.path.isfile(source): raise Exception(source   + ' missing, exiting')
        if not os.path.isfile(region): raise Exception(region   + ' missing, exiting')

        #compile to llvm ir
        clangcmd = CLANG % (source, outsrc)
        subprocess.call(clangcmd, shell=True)

        # run opt pass
        optcmd = OPT % (region, tempfiles[0], outsrc)
        subprocess.call(optcmd, shell=True)

        testcases = TESTCASES[TESTFILES[i]]
        for j in range(0, len(testcases), 2):
            outxml = tempfiles[0] + testcases[j] 
            corxml = TESTFILES[i] + testcases[j]

            expect = xmlgetvariableinfo(corxml)
            actual = xmlgetvariableinfo(outxml)

            status = "%s%s" % (testcases[j+1], cmpvars(source+':'+testcases[j], expect, actual))
            sys.stdout.write(status)

runtests()
