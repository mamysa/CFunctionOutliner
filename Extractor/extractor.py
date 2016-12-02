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

    def as_argument(self):
        if self.isfunptr == True:
            return self.type
        return ('%s %s') % (self.type, self.name)

    @staticmethod
    def create(xml):
        name = xml.find('name')
        type = xml.find('type')
        isoutput = xml.find('isoutput')
        isfunptr = xml.find('isfunptr')
        
        #name, ptrl and type are required
        if name == None or type == None:
            raise Exception('Missing variable info');
        
        variable = Variable()
        setattr(variable, 'name', name.text)
        setattr(variable, 'type', type.text)
        setattr(variable, 'isoutput', False)
        setattr(variable, 'isfunptr', False)
        if isoutput != None:
            variable.isoutput = bool(isoutput.text)
        if isfunptr != None:
            variable.isfunptr = bool(isfunptr.text)
        return variable

class Function2:
    def __init__(self):
        self.inputs = []
        self.outputs = []
        self.special_out = []  ## for return / gotos within region. (see below)

        self.funcname   = 'extracted' ## TODO user should be able to specify this
        self.retvalname = '%s_retval' ## name of the structure returned from the function
        self.retvaltype = 'struct %s_struct' ## type name of the structure returned from extracted function

        # if extracted function contains return / goto statements, we need to also return same values
        # from caller function. The idea is to use (flag, value) pairs. After extracted function returns, 
        # we check these flags in the caller and return accordingly.
        self.exitflagname  = '%s_flag_loc%s' 
        self.exitvaluename = '%s_value_loc%s'

    ## add inputs / outputs to the function
    def add_input(self, var):  
        self.inputs.append(var)

    def add_output(self, var): 
        self.outputs.append(var)

    ## returns function definition.
    def get_fn_definition(self):
        args = '' 
        for var in self.inputs: args = args + var.as_argument() + ', '
        args = args.rstrip(', ') 
        rett = self.retvaltype % (self.funcname)
        return ('%s %s(%s) {\n') % (rett, self.funcname, args)

    ## returns correct function call string
    def get_fn_call(self):
        args = ''
        for var in self.inputs: args = args + var.name + ', '
        args = args.rstrip(', ') 
        rett = self.retvalname % (self.funcname)
        return ('%s = %s(%s);\n') % (rett, self.funcname, args)

    # have to also return input arguments. Passing inputs by pointer doesn't work in certain 
    # cases (like struct initializers) and results in UB.
    def declare_return_type(self):
        args = '' 
        for var in self.inputs:  args = args + '\t' + var.as_argument() + ';\n' 
        for var in self.outputs: args = args + '\t' + var.as_argument() + ';\n' 

## TODO specials 

        name = self.retvalname % (self.funcname)
        type = self.retvaltype % (self.funcname)
        return '%s %s {\n%s};\n' % (type, name, args)

    # when function is about to return, we need to store all the return values into 
    # the structure and return said structure
    # stores only inputs / outputs, special exits are handled separately
    def store_retvals_and_return(self):
        args = ''
        rett = self.retvalname % (self.funcname)
        for var in self.inputs:  args = args + ('%s.%s = %s;\n' % (rett, var.name, var.name)) 
        for var in self.outputs: args = args + ('%s.%s = %s;\n' % (rett, var.name, var.name)) 
        return '%sreturn %s;\n' % (args, rett)

    # restores variables in the callee. If variable is in output variable list, we also need to
    # define its time
    def restore_retvals(self):
        args = ''
        rett = self.retvalname % (self.funcname)
        for var in self.inputs:  args = args + ('%s = %s.%s;\n' % (var.name, rett, var.name))
        for var in self.outputs: args = args + ('%s = %s.%s;\n' % (var.as_argument(), rett, var.name))

        ## we also need to check for gotos / returns here...
        ## TODO
        return args;

## Function class. 
class Function:
    def __init__(self):
        self.inputargs = [] 
        self.outputargs = [] 
        self.exitedges = []
        self.outputspecial = []
        self.retvalname = 'extractedretval'
        self.retvaltype = 'struct extracted_retval'
        self.exitbool = 'bool_exited_at_loc'
        self.exitvalue = 'val_exited_at_loc'

    def addInput(self, variable):
        self.inputargs.append(variable)

    def addOutput(self, variable):
        self.outputargs.append(variable)

    def addExit(self, exit):
        self.exitedges.append(exit)

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
    # if we have special retvalues, we initialize boolean values to 0.
    def declareReturnValue(self):
        if len(self.outputargs) == 0 and len(self.outputspecial) == 0:
            return ""
        out = '%s %s;\n' % (self.retvaltype, self.retvalname)
        for var in self.outputspecial:
            out = out + '%s.%s_%s = 0;\n' % (self.retvalname, self.exitbool, var[0])
        return out


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
        for var in self.outputspecial:
            out = out + 'char %s_%s;\n' % (self.exitbool, var[0])
            out = out + 'todo %s_%s;\n' % (self.exitvalue, var[0]) 
            #FIXME we need original funcs return type!
        out = out + '};\n'
        return out

    # Have to examine exiting locations of the region. 
    # If region contains return statements, we will need to return original function too. 
    # Same idea with gotos.
    # In case if region contains return statement, we remove it, add a boolean into retval structure, 
    # also save variable returned in said structure, and check whether or not the boolean value 
    # is set in the parent function. 
    def analyzeExitEdges(self, regionloc):
        for exit in self.exitedges:
            temp = regionloc[exit]
            temp = temp.lstrip(' \t')
            temp = temp.rstrip('\n;  ')

            # looking for return statement
            idx = temp.find('return') # yeah this totally cannot break... at all. I said so
            if idx != -1:  
                retval = temp[(idx+len('return')):]
                self.outputspecial.append((exit, 'return', retval)) 
                # replace return statement with structure 
                out = '%s.%s_%s = 1; \n' % (self.retvalname, self.exitbool, exit)
                out = out + '%s.%s_%s = %s;\n' % (self.retvalname, self.exitvalue, exit, retval)
                out = out + 'return %s;\n' % (self.retvalname)
                regionloc[exit] = out

#GLOBALS 
reginfo = None
funinfo = None
regloc = {}
funloc = {} 
special_exitlocs = []

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

# Extracted regions sometimes do not include a closing brace so we need to find the line that
# has it in the rest of the function. Assumes that closing brace is located on its own line.
def regionClosingBrace():
    global reginfo
    numopeningbraces = 0
    numclosingbraces = 0
    for line in regloc.values():
        print(line)
        numopeningbraces = numopeningbraces + line.count('{')
        numclosingbraces = numclosingbraces + line.count('}')
    # take the first line that has closing brace in it...
    if numclosingbraces < numopeningbraces:
        for i in range(reginfo.end + 1, funinfo.end): 
            temp = funloc[i].lstrip(' \t')
            temp = temp.rstrip(' \n')
            # just an empty line
            if len(temp) == 0:
                continue
            #found an empty line with brace, use this.
            if temp == '}':
                regloc[i] = funloc[i]
                funloc[i] = None
                reginfo.end = i;
                break
            #line filled with something, we should stop
            if len(temp) != 0:
                break

#Boring parsing stuff
def parseLLVMData():
    global reginfo 
    global funinfo 
    func2 = Function2()
    tree = ET.parse(sys.argv[1]);
    for child in tree.getroot():
        if (child.tag == 'region'):
            reginfo = LocInfo.create(child)
        if (child.tag == 'function'):
            funinfo = LocInfo.create(child)
        if (child.tag == 'variable'):
            variable = Variable.create(child)
            if (variable.isoutput):
                func2.add_output(variable)
            else:
                func2.add_input(variable)
        if (child.tag == 'regionexit'):
            special_exitlocs.append(int(child.text))
    return func2

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
    func.analyzeExitEdges(regloc)

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
    #sys.stdout.write('}\n')


def main():
    if len(sys.argv) != 3:
        sys.stdout.write("Expected two arguments, actual: " + str(len((sys.argv))-1) + "\n")
        sys.exit(1)
    func2 = parseLLVMData()
    print(func2.get_fn_definition())
    print(func2.get_fn_call())
    print(func2.declare_return_type())
    print(func2.store_retvals_and_return())
    print(func2.restore_retvals())
    #parseSrcFile()
    #regionClosingBrace()
    #extract()    


main()
