import sys
import re
import xml.etree.cElementTree as ET


class LocInfo:
    @staticmethod
    def create(xml):
        start = xml.find('start')
        end = xml.find('end')
        if start == None or start == None:
            raise Exception("Missing region info.") 

        locinfo = LocInfo()
        setattr(locinfo, 'start', int(start.text)) 
        setattr(locinfo, 'end',   int(end.text)  ) 
        return locinfo 

    def __repr__(self):
        return '<LocInfo start:%s end:%s>' % (self.start, self.end)

#######################################
class Variable: 
    def __repr__(self):
        return '<Variable name:%s type:%s ptrl:%s isoutput:%s>' % (self.name, self.type, self.ptrl, self.isoutput)

    def gettype(self):
        ptr = ''.join(['*' for i in range(self.ptrl)])
        return '%s %s' % (self.type, ptr)

    def getname(self):
        return self.name

    def tostring(self):
        if self.isoutput:
            return ""
        if not self.isoutput: 
            ## we pass a pointer to the variable into the function
            ptr = ''.join(['*' for i in range(self.ptrl + 1)])
            return '%s %s%s' % (self.type, ptr, self.name)

    @staticmethod
    def create(xml):
        name = xml.find('name')
        ptrl = xml.find('ptrl')
        type = xml.find('type')
        isoutput = xml.find('isoutput')
        
        #name, ptrl and type are required
        if name == None or ptrl == None or type == None:
            raise Exception('Missing variable info');
        
        variable = Variable()
        setattr(variable, 'name', name.text)
        setattr(variable, 'type', type.text)
        setattr(variable, 'ptrl', int(ptrl.text))
        setattr(variable, 'isoutput', False)
        if (isoutput != None):
            variable.isoutput = bool(isoutput.text)
        return variable

## Function class. 
class Function:
    def __init__(self):
        self.inputargs = [] 
        self.outputargs = [] 

    def addInput(self, variable):
        self.inputargs.append(variable)

    def addOutput(self, variable):
        self.outputargs.append(variable)

    def gettype():
        if len(self.outputargs) == 0:
            return 'void'
        return 'todo'

    def getFnDecl(self):
        args = ''
        for variable in self.inputargs:
            args = args + variable.gettype() + '*' + variable.getname() + ', '
        args = args.rstrip(', ')
        type = self.gettype()
        return '%s extracted(%s) {' % (type, args)

    def getFnCall():
        for variable in self.inputargs:
            args = args + '&' + variable.getname() + ', '
        args = args.rstrip(', ')
        return 'extracted(%s);' % (args) ##TODO retvals

#GLOBALS 
regionInfo = None
functionInfo = None
regionlines = {}
functionlines = {}
function = Function() 

# dereferences function inputs. The problem with this approach is variable scope.
# c doesn't prevent variable shadowing and we might end up having two variables with the same 
# name. How to fix: have different names for each variable... :) 
def findIdentifier(line, variable):
    regex = re.compile('[a-zA-Z0-9_]')
    idx = line.find(variable.name)
    while idx != -1:
        # if whatever comes before or after the variable still contains alphanumerics, 
        # then variable itself is not exact match.
        begin = idx - 1
        end   = idx + len(variable.name) 
        if begin >= 0 and regex.match(line[begin]):
            idx = line.find(variable.name, end)
            continue
        if end < len(line) and regex.match(line[end]):
            idx = line.find(variable.name, end)
            continue

        newline = ""
        for i in range(idx):
           newline = newline + line[i]
        newline = newline + '(*' + variable.name + ')'
        for i in range(end, len(line)):
           newline = newline + line[i]
        line = newline
        idx = line.find(variable.name, end + 2)
    return line

def parseLLVMData():
    global regionInfo 
    global functionInfo 

    tree = ET.parse(sys.argv[1]);
    for child in tree.getroot():
        if (child.tag == 'region'):
            regionInfo = LocInfo.create(child)
        if (child.tag == 'function'):
            functionInfo = LocInfo.create(child)
        if (child.tag == 'variable'):
            variable = Variable.create(child)
    print(regionInfo)

def parseSrcFile(fileInfo):
    function = {}
    region = {}

    f = open(sys.argv[2])
    line = f.readline()
    linenum = 1
    while line != '':
        if linenum >= fileInfo.fstart and linenum <= fileInfo.fend:
            if linenum >= fileInfo.rstart and linenum <= fileInfo.rend:
                region[linenum] = line
            else:
                function[linenum] = line
        line = f.readline()
        linenum = linenum + 1
    f.close()

## the part where interesting things happen.  
def extract(srcdata, llvmdata):
    fileinfo = llvmdata[0]
    variables = llvmdata[1]
    regionEdges = llvmdata[2] ## TODO nothing here yet
    function = srcdata[0]
    region = srcdata[1]
    #TODO analyze areas of interest - i.e. lines that point to external basicblock - we might have 
    # return statement or goto statement there. 


    ## for now, i am disregarding those things and assuming that extracted region does not have any 
    ## outputs and does not have any return / goto statements 
    funcargs = ''
    callargs = ''
    for v in variables: 
        funcargs = funcargs + v.tostring() + ', ' ## for function declaration
        callargs = callargs + '&' + v.name + ', ' ## for function call


    funcargs = funcargs.rstrip(', ')
    callargs = callargs.rstrip(', ')
    funcdecl = 'void extracted(%s) {\n' % (funcargs)
    funccall = 'extracted(%s);\n' % (callargs)

    for (linenum, line) in region.items():
        for variable in variables:
            line = findIdentifier(line, variable)
        region[linenum] = line
        

    
    ## write extracted function 
    #extracted region's first line should have only one tab. Readjust tab count as necessary
    #TODO


    tabcount = region[fileinfo.rstart].count('\t') 
    sys.stdout.write(funcdecl)
    for (key, value) in region.items():
        sys.stdout.write(value)
    sys.stdout.write('}\n')

    #print('\n')
    ## write original function
    #for i in range(fileinfo.fstart, fileinfo.rstart):
    #    sys.stdout.write(function[i])
    #sys.stdout.write(funccall)
    #for i in range(fileinfo.rend + 1, fileinfo.fend + 1):
    #    sys.stdout.write(function[i])
    #sys.stdout.write('}\n')


def main():
    if len(sys.argv) != 3:
        sys.stdout.write("Expected two arguments, actual: " + str(len((sys.argv))-1) + "\n")
        sys.exit(1)
    parseLLVMData()
    

    #llvmdata = parseLLVMInfo()
    #srcdata = parseSrcFile(llvmdata[0])
    #extract(srcdata, llvmdata)

main()
