import sys
import re
import xml.etree.cElementTree as ET

class FileInfo:
    def __init__(self):
        self.funinfo = None  ## starting ending linenumbers of a function
        self.reginfo = None  ## starting ending linenumbers of a regioon
        self.funloc = {} 
        self.regloc = {}     ## actual lines for both region and a function
        self.exitlocs = []   ## line numbers corresponding to exiting edges of the region.
        self.vars = []       ## variable list
        self.funrettype = "" ## return type of the function
        self.funname = ""    ## name of the extracted function

    # in case if region starts with the same line as the function we are extracting from, 
    # it means that function header is also a part of a region and has to be separated from 
    # function body.
    # This is done by grabbing everything up to and including the first opening brace and updating
    # region bounds accordingly.
    def try_separate_func_header(self):
        # nothing to do here bailing
        if self.reginfo.start != self.funinfo.start: return

        for i in range(self.reginfo.start, self.reginfo.end + 1):
            n = self.regloc[i].count('{')
            if n != 0: 
                #ensure that we don't have anything following the opening brace. 
                assert(self.regloc[i].split('{')[1].rstrip(' \n') == '')
                self.funloc[i] = self.regloc[i]
                del self.regloc[i]
                self.reginfo.start = i + 1
                break
        assert(self.reginfo.start != self.reginfo.end)

    # Extracted regions sometimes do not include a closing brace so we need to find the line that
    # has it in the rest of the function. Assumes that closing brace is located on its own line.
    # We are only expecting a single brace
    def region_find_closing_brace(self):
        numopeningbraces = 0
        numclosingbraces = 0
        for line in self.regloc.values():
            ## remove literal strings / chars from the line. 
            line = re.sub(r'\'(.)*\'', '', line, re.M | re.I)
            line = re.sub(r'\"(\\.|[^"])*\"', '', line, re.M | re.I) 
# count number of braces. braces are now expected to be balanced aside from the last one closing
# off the region. 
            numopeningbraces = numopeningbraces + line.count('{')
            numclosingbraces = numclosingbraces + line.count('}')
        # take the first line that has closing brace in it...
        if numclosingbraces < numopeningbraces:
            for i in range(self.reginfo.end + 1, self.funinfo.end): 
                temp = self.funloc[i].lstrip(' \t')
                temp = temp.rstrip(' \n')
                # just an empty line
                if len(temp) == 0:
                    continue
                #found an empty line with brace, use this.
                if temp == '}':
                    self.regloc[i] = self.funloc[i]
                    del self.funloc[i] 
                    self.reginfo.end = i;
                    break
                #line filled with something, we should stop
                if len(temp) != 0:
                    break

    def function_add_closing_brace(self):
        numopeningbraces = 0
        numclosingbraces = 0
        for key in sorted(self.funloc.keys()):
            line = re.sub(r'\'(.)*\'', '', self.funloc[key], re.M | re.I)
            line = re.sub(r'\"(\\.|[^"])*\"', '', line, re.M | re.I) 
            numopeningbraces = numopeningbraces + line.count('{')
            numclosingbraces = numclosingbraces + line.count('}')
        if numclosingbraces < numopeningbraces:
            self.funinfo.end = self.funinfo.end + 1
            self.funloc[self.funinfo.end] = '}\n'

    def extract(self):
        function = Function(self.funname, self.funrettype)
        for var in self.vars:
            function.add_variable(var)

        for loc in sorted(self.exitlocs):
            function.check_exit_loc(self.regloc, loc)

        sys.stdout.write(function.declare_return_type())
        sys.stdout.write(function.get_fn_definition())
        sys.stdout.write(function.define_return_value())
        for num in sorted(self.regloc.keys()):
            sys.stdout.write(self.regloc[num])
        if len(function.special_out) == 0:
            sys.stdout.write(function.store_retvals_and_return())
        sys.stdout.write('}\n\n')

        for i in range(self.funinfo.start, self.reginfo.start):
            sys.stdout.write(self.funloc[i])
        sys.stdout.write(function.get_fn_call())  
        sys.stdout.write(function.restore_retvals()) ## if function is not void, restore all variables
        for i in range(self.reginfo.end + 1, self.funinfo.end + 1):
            sys.stdout.write(self.funloc[i])




class LocInfo:
    def __init__(self, start, end):
        self.start = start
        self.end = end

    def between(self, num):
        return num >= self.start and num <= self.end

    @staticmethod
    def create(xml):
        start = xml.find('start')
        end = xml.find('end')
        if start == None or end == None:
            raise Exception("Missing region info.") 

        locinfo = LocInfo(int(start.text), int(end.text))
        return locinfo 

    def __repr__(self):
        return '<LocInfo start:%s end:%s>' % (self.start, self.end)

    def __eq__(self, other):
        return self.start == other.start and self.end == other.end

#######################################
class Variable: 
    def __init__(self, name, type):
        self.name = name
        self.type = type
        self.isfunptr = False
        self.isoutput = False

    def __repr__(self):
        return '<Variable name:%s type:%s isoutput:%s>' % (self.name, self.type, self.isoutput)

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

class Function:
    def __init__(self, funname, funrettype):
        self.inputs = []
        self.outputs = []
        self.special_out = [] # for return / gotos within region. (see below)

        self.funname    = funname ## name of the extracted function
        self.funrettype = funrettype ## return type of the original function
        self.retvalname = '%s_retval' ## name of the structure returned from the function
        self.retvaltype = 'struct %s_struct' ## type name of the structure returned from extracted function

        # if extracted function contains return / goto statements, we need to also return same values
        # from caller function. The idea is to use (flag, value) pairs. After extracted function returns, 
        # we check these flags in the caller and return accordingly.
        self.exitflagname  = '%s_flag_loc%s' 
        self.exitvaluename = '%s_value_loc%s'

    ## TODO check for const qualified variables. 
    def add_variable(self, var):
        if not var.isoutput: self.inputs.append(var)
        if var.isoutput: self.outputs.append(var)

    # replace return / goto statement in the region with setting flag to 1 and setting value to whatever
    # comes on the rhs of the return statement.
    # we expect return/goto statements formatted in a certain way.
    def check_exit_loc(self, regloc, loc):
        temp = regloc[loc]
        temp = temp.lstrip('\t ')
        temp = temp.rstrip('\n; ')

        retstmt = line_contains(temp, 'return')
        if retstmt != (None, None):
            flg = Variable(self.exitflagname  % (self.funcname, loc), 'char')
            val = Variable(self.exitvaluename % (self.funcname, loc), funrettype)
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
        if gotostmt != (None, None):
            flg = Variable(self.exitflagname  % (self.funcname, loc), 'char')
            val = Variable(gotostmt[1], 'label') # this should be label
            self.special_out.append(IfCondition(flg, val, 'goto')) 

            ## store flag
            rett = self.retvalname % (self.funcname)
            v1 = '%s.%s = %s;\n' % (rett, flg.name , '1')
            regloc[loc] = '%s%s' % (v1, self.store_retvals_and_return())    


    ## returns function definition.
    def get_fn_definition(self):
        args = '' 
        for var in self.inputs: args = args + var.as_argument() + ', '
        args = args.rstrip(', ') 
        rett = self.retvaltype % (self.funname)
        return ('%s %s(%s) {\n') % (rett, self.funname, args)

    ## returns correct function call string
    def get_fn_call(self):
        args = ''
        for var in self.inputs: args = args + var.name + ', '
        args = args.rstrip(', ') 
        retn = self.retvalname % (self.funname)
        rett = self.retvaltype % (self.funname)
        return ('%s %s = %s(%s);\n') % (rett, retn, self.funname, args)

    # have to also return input arguments. Passing inputs by pointer doesn't work in certain 
    # cases (like struct initializers) and results in UB.
    def declare_return_type(self):
        args = '' 
        for var in self.inputs:  args = args + '\t' + var.as_argument() + ';\n' 
        for var in self.outputs: args = args + '\t' + var.as_argument() + ';\n' 

        for cond in self.special_out:
            args = args + '\t' + cond.cond.as_argument() + ';\n'
            if (cond.stmt == 'return'): args = args + '\t' + cond.var.as_argument() + ';\n'

        type = self.retvaltype % (self.funname)
        return '%s {\n%s};\n\n' % (type, args)

    # defines return value in the beginning of the extracted function and sets 
    # special return flags to 0 if those exist
    def define_return_value(self):
        name = self.retvalname % (self.funname)
        type = self.retvaltype % (self.funname)
        out  = '\t%s %s;\n' % (type, name)
        
        for cond in self.special_out:
            out = out + ('\t%s.%s = 0;\n' % (name, cond.cond.name)) 
        return out + '\n'


    # when function is about to return, we need to store all the return values into 
    # the structure and return said structure
    # stores only inputs / outputs, special exits are handled separately
    def store_retvals_and_return(self):
        args = ''
        rett = self.retvalname % (self.funname)
        for var in self.inputs:  args = args + ('%s.%s = %s;\n' % (rett, var.name, var.name)) 
        for var in self.outputs: args = args + ('%s.%s = %s;\n' % (rett, var.name, var.name)) 
        return '%sreturn %s;\n' % (args, rett)

    # restores variables in the callee. If variable is in output variable list, we also need to
    # define its time
    def restore_retvals(self):
        args = ''
        rett = self.retvalname % (self.funname)
        for var in self.inputs:  args = args + ('%s = %s.%s;\n' % (var.name, rett, var.name))
        for var in self.outputs: args = args + ('%s = %s.%s;\n' % (var.as_argument(), rett, var.name))

        # generate if statements if applicable
        out = ''
        for cond in self.special_out:
            var = cond.var.name
            if cond.stmt == 'return': var = '%s.%s' % (rett, cond.var.name)
            out = out + 'if (%s.%s) { %s %s; }\n' % (rett, cond.cond.name, cond.stmt, var)
        return args + out;

        
def extract2(function):
    for loc in special_exitlocs:
        function.check_exit_loc(loc)

    sys.stdout.write(function.declare_return_type())
    sys.stdout.write(function.get_fn_definition())
    sys.stdout.write(function.define_return_value())
    for num in sorted(regloc.keys()):
        sys.stdout.write(regloc[num])
    if len(function.special_out) == 0:
        sys.stdout.write(function.store_retvals_and_return())
    sys.stdout.write('}\n\n')

    for i in range(funinfo.start, reginfo.start):
        sys.stdout.write(funloc[i])
    sys.stdout.write(function.get_fn_call())  
    sys.stdout.write(function.restore_retvals()) ## if function is not void, restore all variables
    for i in range(reginfo.end + 1, funinfo.end + 1):
        sys.stdout.write(funloc[i])


#FIXME this will break quite a lot.
def line_contains(line, string):
    idx = line.find(string) 
    if idx != -1:
        ## TODO extract things on the LHS too as we might have multiple statements on the same line...
        rhs = line[(idx+len(string)):]
        return (string, rhs) 
    return (None, None)

#Boring parsing stuff
# Read XML file.
def parse_xml(fileinfo):
    tree = ET.parse(sys.argv[1]);
    for child in tree.getroot():
        if (child.tag == 'funcname'):   fileinfo.funname = child.text
        if (child.tag == 'funcreturntype'): fileinfo.funrettype = child.text
        if (child.tag == 'regionexit'): fileinfo.exitlocs.append(int(child.text))
        if (child.tag == 'region'):     fileinfo.reginfo = LocInfo.create(child)
        if (child.tag == 'function'):   fileinfo.funinfo = LocInfo.create(child)
        if (child.tag == 'variable'):   fileinfo.vars.append(Variable.create(child))

# Read original source file into two different dictionaries.
def parse_src(fileinfo):
    f = open(sys.argv[2])
    line = f.readline()
    linenum = 1
    while line != '':
        if fileinfo.funinfo.between(linenum):
            if fileinfo.reginfo.between(linenum): fileinfo.regloc[linenum] = line
            else: fileinfo.funloc[linenum] = line
        line = f.readline()
        linenum = linenum + 1
    f.close()

def main():
    if len(sys.argv) != 3:
        sys.stdout.write("Expected two arguments, actual: " + str(len((sys.argv))-1) + "\n")
        sys.exit(1)
    fileinfo = FileInfo()
    parse_xml(fileinfo)
    parse_src(fileinfo)

    fileinfo.try_separate_func_header()
    fileinfo.region_find_closing_brace()
    fileinfo.function_add_closing_brace()
    fileinfo.extract()
    #extract2(func2)

if __name__ == '__main__':
    main()
