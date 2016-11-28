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
        self.retvalname = 'extractedretval'
        self.retvaltype = 'struct extracted_retval'

    def addInput(self, variable):
        self.inputargs.append(variable)

    def addOutput(self, variable):
        self.outputargs.append(variable)

    def gettype(self):
        if len(self.outputargs) == 0:
            return 'void'
        return 'struct ' + self.retvalname;

    def getFnDecl(self):
        args = ''
        for variable in self.inputargs:
            args = args + variable.gettype() + '*' + variable.getname() + ', '
        args = args.rstrip(', ')
        type = self.gettype()
        return '%s extracted(%s) {\n' % (type, args)

    def getFnCall(self):
        args = ''
        for variable in self.inputargs:
            args = args + '&' + variable.getname() + ', '
        args = args.rstrip(', ')
        return 'extracted(%s);\n' % (args) ##TODO retvals

    # in case if extracted function returns something, return structure needs to be defined first. 
    # returns 'struct struct_type retval' or nothing if the fuction does not return anything
    def declareReturnValue(self):
        if len(self.outputargs) == 0:
            return ""
        return self.retvaltype + ' ' + self.retvalname + ';\n'

    # if function returns, we need to assign to values in the structure where appropriate.
    def setReturnValues(self):
        if len(self.outputargs) == 0:
            return ""
        out = ''
        for var in self.outputargs:
            temp = '%s.%s = %s;\n' % (self.retvalname, var.getname(), var.getname())
            out = out + temp
        retstmt = 'return %s;\n' % self.retvalname;
        out = out + retstmt
        return out

    #if extracted function returns something, we have to declare these variables 
    # in the original function and assign correct return values.
    def restoreReturnedValues(self): 
        out = ''
        for var in self.outputargs:
            st = '%s%s = %s.%s;\n' % (var.gettype(), var.getname(), self.retvalname, var.getname())
            out = out + st
        return out

    def getReturnTypeDefinition(self):
        out = 'struct ' + self.retvalname + ' {\n'
        for var in self.outputargs:
            out = out + '%s%s;\n' % (var.gettype(), var.getname())
        out = out + '};\n'
        return out


#GLOBALS 
reginfo = None
funinfo = None
regloc = {}
funloc = {} 
func = Function() 
regedges = []

# dereferences function inputs. The problem with this approach is that C permits two variables
# have the same name in two overlapping scopes, causing variable shadowing.. 
# How to fix: have different names for each variable... :)  ...and don't have things like 
# the following:
# int x = 12;
# {
#   char x = 255; ...;
# } ...
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

#Boring parsing stuff
def parseLLVMData():
    global reginfo 
    global funinfo 

    tree = ET.parse(sys.argv[1]);
    for child in tree.getroot():
        if (child.tag == 'region'):
            reginfo = LocInfo.create(child)
        if (child.tag == 'function'):
            funinfo = LocInfo.create(child)
        if (child.tag == 'variable'):
            variable = Variable.create(child)
            if (variable.isoutput):
                func.addOutput(variable)
            else:
                func.addInput(variable)

def parseSrcFile():
    f = open(sys.argv[2])
    line = f.readline()
    linenum = 1
    while line != '':
        if linenum >= funinfo.start and linenum <= funinfo.end:
            if linenum >= reginfo.start and linenum <= reginfo.end:
                regloc[linenum] = line
            else:
                funloc[linenum] = line
        line = f.readline()
        linenum = linenum + 1
    f.close()

## the part where interesting things happen.  
def extract():
    #TODO analyze areas of interest - i.e. lines that point to external basicblock - we might have 
    # return statement or goto statement there. 
    ## for now, i am disregarding those things and assuming that extracted region does not have any 
    ## outputs and does not have any return / goto statements 



    ## dereference arguments in function body
    for (linenum, line) in regloc.items():
        for variable in func.inputargs:
            line = findIdentifier(line, variable)
        regloc[linenum] = line
        
    #TODO copy everything from original file before given function and after 
    sys.stdout.write(func.getReturnTypeDefinition())
    sys.stdout.write(func.getFnDecl())
    sys.stdout.write(func.declareReturnValue())
    for line in regloc.values():
        sys.stdout.write(line)
    sys.stdout.write(func.setReturnValues())
    sys.stdout.write('}\n')

    # write original function
    for i in range(funinfo.start, reginfo.start):
        sys.stdout.write(funloc[i])
    sys.stdout.write(func.getFnCall())  
    sys.stdout.write(func.restoreReturnedValues()) ## if function is not void, restore all variables
    for i in range(reginfo.end + 1, funinfo.end + 1):
        sys.stdout.write(funloc[i])
    sys.stdout.write('}\n')


def main():
    if len(sys.argv) != 3:
        sys.stdout.write("Expected two arguments, actual: " + str(len((sys.argv))-1) + "\n")
        sys.exit(1)
    parseLLVMData()
    parseSrcFile()
    extract()    

    #llvmdata = parseLLVMInfo()
    #srcdata = parseSrcFile(llvmdata[0])
    #extract(srcdata, llvmdata)

main()
