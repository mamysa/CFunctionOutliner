import sys
import re

class FileInfo:
    def __init__(self):
        self.fstart = -1
        self.fend = -1 
        self.rstart = -1
        self.rend = -1

    def valid(self):
        return self.fstart != -1 and self.fend != -1 and self.rstart != -1 and self.rend != -1

    def __repr__(self):
        return '<FileInfo rstart:%s rend:%s fstart:%s fend:%s>' % (self.rstart, self.rend, self.fstart,self.fend)

class Variable: 
    def __init__(self, _name, _type, _ptrl, _isoutput):
        self.isoutput = _isoutput 
        self.name = _name 
        self.type = _type 
        self.ptrl = _ptrl 

    def valid(self):
        return self.name != "" and self.type != "" and self.ptrl != -1

    def __repr__(self):
        return '<Variable name:%s type:%s ptrl:%s isoutput:%s>' % (self.name, self.type, self.ptrl, self.isoutput)

    def gettype(self):
        ptr = ''.join(['*' for i in range(self.ptrl)])
        return '%s %s' % (self.type, ptr)

    def tostring(self):
        if self.isoutput:
            return ""
        if not self.isoutput: 
            ## we pass a pointer to the variable into the function
            ptr = ''.join(['*' for i in range(self.ptrl + 1)])
            return '%s %s%s' % (self.type, ptr, self.name)


class Function:
    def __init__(self, _type, inputargs, outputargs):
        self.inputargs = inputargs
        self.outputargs = outputargs

    def gettype():
        if len(self.outputargs) == 0:
            return 'void'


##
def findIdentifier(line, variable):
    regex = re.compile('[a-zA-Z0-9_]')
    idx = line.find(variable.name)
    print (variable.name, idx)
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


def sanitize(line):
    line = line.strip(" ")
    line = line.replace(' ' , '')
    line = line.replace('\n', '')
    return line


def parseFileInfo(f):
    fileInfo = FileInfo()

    line = f.readline()
    while line.find('}') == -1: 
        if line == '':
            sys.stdout.write('Unexpected end of file, exiting...\n')
            sys.exit(1)
        line = sanitize(line)
        line = line.split(":") 
        if len(line) != 2:
            sys.stdout.write('Bad line: ' + str(line) + '\n')
            sys.exit(1)
        if line[0] == 'REGIONSTART': fileInfo.rstart = int(line[1])
        if line[0] == 'REGIONEND'  : fileInfo.rend   = int(line[1])
        if line[0] == 'FUNCSTART'  : fileInfo.fstart = int(line[1])
        if line[0] == 'FUNCEND'    : fileInfo.fend   = int(line[1])
        line = f.readline()

    if not fileInfo.valid():
        sys.stdout.write('invalid region info object\n')
        sys.exit(1)

    return fileInfo 



def parseVariable(f):
    variable = Variable("", "", -1, True)

    line = f.readline()
    while line.find('}') == -1: 
        if line == '':
            sys.stdout.write('Unexpected end of file, exiting...\n')
            sys.exit(1)

        line = line.strip(" ")
        #line = sanitize(line)
        line = line.replace('\n', '')
        line = line.split(":") 
        if len(line) != 2:
            sys.stdout.write('Bad line: ' + str(line) + '\n')
            sys.exit(1)
        line[0] = line[0].strip(" ")
        line[1] = line[1].strip(" ")
        if line[0] == 'bINPUT': variable.isoutput = False 
        if line[0] == 'NAME'  : variable.name = line[1]
        if line[0] == 'TYPE'  : variable.type = line[1]
        if line[0] == 'PTRL'  : variable.ptrl = int(line[1])
        line = f.readline()

    if not variable.valid():
        sys.stdout.write('invalid variable object\n')
        sys.exit(1)

    return variable


def parseLLVMInfo():
    fileInfo = None
    variables = []  
    regionEdges = []

    f = open(sys.argv[1])
    line = f.readline()
    while line != '':
        line = sanitize(line)
        if line.find('FileInfo{') != -1: fileInfo = parseFileInfo(f)
        if line.find('Variable{') != -1: variables.append(parseVariable(f))
        line = f.readline()

    f.close()
    return (fileInfo, variables, regionEdges)


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
    return (function, region)


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

    line = '12+outptr1+outptr1+outptr2+r+b+g'
    for variable in variables:
        line = findIdentifier(line, variable)
    
    print(line)
        

    
    ## write extracted function 
    #extracted region's first line should have only one tab. Readjust tab count as necessary
    #TODO


    #tabcount = region[fileinfo.rstart].count('\t') 
    #sys.stdout.write(funcdecl)
    #for (key, value) in region.items():
    #    sys.stdout.write(value)
    #sys.stdout.write('}\n')

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

    llvmdata = parseLLVMInfo()
    srcdata = parseSrcFile(llvmdata[0])
    extract(srcdata, llvmdata)






main()
