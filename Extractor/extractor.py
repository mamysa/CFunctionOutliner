import sys
import re
import xml.etree.cElementTree as ET

#GLOBALS these are very very ... very important. Otherwise they wouldn't be globals... ;) 
reginfo = None
funinfo = None
regloc = {}
funloc = {} 
funrettype = ''   
special_exitlocs = []

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
    def __init__(self, name, type):
        self.name = name
        self.type = type
        self.isfunptr = False
        self.isoutput = False

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
        
        variable = Variable(name.text, type.text)
        if isoutput != None:
            variable.isoutput = bool(isoutput.text)
        if isfunptr != None:
            variable.isfunptr = bool(isfunptr.text)
        return variable

# if condition class for possible return / goto statements inside the region that we have to check 
# in the caller function
class IfCondition:
    def __init__(self, cond, var, stmt):
        self.cond = cond
        self.var  = var
        self.stmt = stmt


class Function2:
    def __init__(self):
        self.inputs = []
        self.outputs = []
        self.special_out = [] # for return / gotos within region. (see below)

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

    # replace return / goto statement in the region with setting flag to 1 and setting value to whatever
    # comes on the rhs of the return statement.
    def check_exit_loc(self, loc):
        temp = regloc[loc]
        temp = temp.lstrip(' \t')
        temp = temp.rstrip('\n;  ')

        retstmt = line_contains(temp, 'return')
        if retstmt != (None, None):
            flg = Variable(self.exitflagname  % (self.funcname, loc), 'char')
            val = Variable(self.exitvaluename % (self.funcname, loc), 'TODO')
            self.special_out.append(IfCondition(flg, val, 'return')) # need this fn_restore_variables procedure
            
            # store these variables in the struct and replace current line with it
            # we do not have to restore all the variables if we are returning.
            rett = self.retvalname % (self.funcname)
            v1 = '%s.%s = %s;\n' % (rett, flg.name , '1')
            v2 = '%s.%s = %s;\n' % (rett, val.name , retstmt[1]) # store retstmt.rhs
            regloc[loc] = '%s%sreturn %s;\n' % (v1, v2, rett)    # should also return 
            return

        ## TODO 
        gotostmt = line_contains(temp, 'goto')
        if goto != (None, None):
            print('we got a goto!!!')


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

        for cond in self.special_out:
            args = args + '\t' + cond.cond.as_argument() + ';\n'
            if (cond.stmt == 'return'): args = args + '\t' + cond.var.as_argument() + ';\n'

        name = self.retvalname % (self.funcname)
        type = self.retvaltype % (self.funcname)
        return '%s %s {\n%s};\n' % (type, name, args)

    # defines return value in the beginning of the extracted function and sets 
    # special return flags to 0 if those exist
    def define_return_value(self):
        name = self.retvalname % (self.funcname)
        type = self.retvaltype % (self.funcname)
        out  = '\t%s %s;\n' % (type, name)
        
        for cond in self.special_out:
            out = out + ('\t%s.%s = 0;\n' % (name, cond.cond.name)) 
        return out


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

        # generate if statements if applicable
        out = ''
        for cond in self.special_out:
            out = 'if(%s) { %s %s; }\n' % (cond.cond.name, cond.stmt, cond.var)
        return out + args;

        
def extract2(function):
    for loc in special_exitlocs:
        function.check_exit_loc(loc)

    sys.stdout.write(function.declare_return_type())
    sys.stdout.write(function.get_fn_definition())
    sys.stdout.write(function.define_return_value())

    #for line in regloc.values():
    #    sys.stdout.write(line)
    #sys.stdout.write(func.setReturnValues())
    #sys.stdout.write('}\n')


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
def region_find_closing_brace():
    global reginfo
    numopeningbraces = 0
    numclosingbraces = 0
    for line in regloc.values():
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

#FIXME this will break quite a lot.
def line_contains(line, string):
    idx = line.find(string) 
    if idx != -1:
        ## TODO extract things on the LHS too as we might have multiple statements on the same line...
        rhs = line[(idx+len(string)):]
        return (string, rhs) 
    return (None, None)


#Boring parsing stuff
def parseLLVMData():
    global reginfo 
    global funinfo 
    global special_exitlocs
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
    special_exitlocs = sorted(special_exitlocs)
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
    parseSrcFile()
    region_find_closing_brace()
    #extract()    
    extract2(func2)


if __name__ == '__main__':
    main()
