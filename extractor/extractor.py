import sys
import re
import argparse
import xml.etree.cElementTree as ET

# global command line arguments
CLIARGS = None

# Class containing lines of code that we will be operating on, as well as all data output by llvm.
class FileInfo:
    def __init__(self):
        self.funinfo = None   # starting ending linenumbers of a function
        self.reginfo = None   # starting ending linenumbers of a regioon
        self.funloc = {} 
        self.regloc = {}      # actual lines for both region and a function
        self.prefunc  = []    # lines before the function
        self.postfunc = []    # lines after  the function 
        self.exitlocs = []    # line numbers corresponding to exiting edges of the region.
        self.vars = []        # variable list
        self.funrettype = ""  # return type of the function
        self.funname = ""     # name of the extracted function
        self.toplevel = False # is the region a function already?

    # in case if region starts with the same line as the function we are extracting from, 
    # it means that function header is also a part of a region and has to be separated from 
    # function body.
    # This is done by grabbing everything up to and including the first opening brace and updating
    # region bounds accordingly.
    def try_separate_func_header(self):
        if self.reginfo.start != self.funinfo.start: return
        for i in range(self.reginfo.start, self.reginfo.end + 1):
            n = self.regloc[i].count('{')
            if n != 0: 
                #ensure that we don't have anything following the opening brace. 
                if self.regloc[i].split('{')[1].rstrip(' \n') != '':
                    raise Exception('Non-empty string after closing brace!')
                self.funloc[i] = self.regloc[i]
                del self.regloc[i]
                self.reginfo.start = i + 1
                break
        if self.reginfo.start == self.reginfo.end:
            raise Exception('Empty region!')


    # Extracted regions sometimes do not include a closing brace so we need to find the line that
    # has it in the rest of the function. Assumes that closing brace is located on its own line.
    # We are only expecting a single brace
    def region_find_closing_brace(self):
        (numopeningbraces, numclosingbraces) = brace_count(self.regloc)
        while numclosingbraces < numopeningbraces:
            for i in range(self.reginfo.end + 1, self.funinfo.end + 1): 
                temp = self.funloc[i].lstrip(' \t')
                temp = temp.rstrip(' \n')
                if len(temp) == 0: continue 

                #found an empty line with brace, use this.
                if temp == '}':
                    self.regloc[i] = self.funloc[i]
                    del self.funloc[i] 
                    self.reginfo.end = i
                    numclosingbraces = numclosingbraces + 1
                    break

                # If this happens, manually edit XML file and set the correct ending line
                if len(temp) != 0: 
                    raise Exception('Could not find closing brace!')

        # save brace numbers for function header insertion!
        self.reginfo.closingbracenum = numclosingbraces
        self.reginfo.openingbracenum = numopeningbraces 

    # if function is missing closing braces, add the number necessary.
    # if we are appending the rest of the src to the extracted region, we do not need to do this. 
    def function_add_closing_brace(self):
        if CLI_ARGS.append: return 

        (numopeningbraces, numclosingbraces) = brace_count(self.funloc)
        while numclosingbraces < numopeningbraces:
            self.funinfo.end = self.funinfo.end + 1
            numclosingbraces = numclosingbraces + 1
            self.funloc[self.funinfo.end] = '}\n'

    def extract(self):
        function = Function(self.funname, self.funrettype)
        for var in self.vars:
            function.add_variable(var)

        for loc in sorted(self.exitlocs):
            function.check_exit_loc(self.regloc, loc)

        # prepend stuff if flag is set
        for loc in self.prefunc:  
            sys.stdout.write(loc)

        sys.stdout.write(function.declare_return_type(self.toplevel))
        sys.stdout.write(function.get_fn_definition(self.toplevel))
        sys.stdout.write(function.define_return_value(self.toplevel))
        for num in sorted(self.regloc.keys()):
            sys.stdout.write(self.regloc[num])
        sys.stdout.write(function.store_retvals_and_return(self.toplevel))
        
        # after inserting function header number of braces will be unbalanced, insert closing 
        # brace if necessary.
        if self.reginfo.closingbracenum == self.reginfo.openingbracenum: 
            sys.stdout.write('}\n\n')  
        else: 
            sys.stdout.write('\n\n')  

        for i in range(self.funinfo.start, self.reginfo.start):
            sys.stdout.write(self.funloc[i])
        sys.stdout.write(function.get_fn_call(self.toplevel))  
        sys.stdout.write(function.restore_retvals(self.toplevel)) ## if function is not void, restore all variables
        for i in range(self.reginfo.end + 1, self.funinfo.end + 1):
            sys.stdout.write(self.funloc[i])

        # append the rest of the source if we have to
        for loc in self.postfunc: 
            sys.stdout.write(loc)


class LocInfo:
    def __init__(self, start, end):
        self.start = start
        self.end = end
        self.closingbracenum = 0
        self.openingbracenum = 0

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
        self.isstatic = False
        self.isconstq = False
        self.isarrayt = False

    def __repr__(self):
        return '<Variable name:%s type:%s isoutput:%s>' % (self.name, self.type, self.isoutput)

    def as_function_argument(self):
        if self.isfunptr: return self.type
        return ('%s %s') % (self.type, self.name)

    # have to get rid of const qualifiers
    # Should not allow input constants to be in the struct.
    def as_struct_member(self):
        if not self.isoutput and self.isconstq: return ''
        if not self.isoutput and self.isarrayt: return ''
        ntype = list(filter(lambda x: x != 'const', self.type.split(' ')))
        ntype = ' '.join(ntype).rstrip(' ')
        if self.isfunptr: return '\t%s;\n' % ntype
        return '\t%s %s;\n' % (ntype, self.name)

    # Declares a variable and initializes it from struct. Static variables have to be initialized to some constant first.
    def declare_and_initialize(self, struct):
        assert(self.isoutput)
        if self.isstatic: return  'static %s %s = 0;\n%s = %s.%s;\n' % (self.type, self.name, self.name, struct, self.name)
        if self.isfunptr: return '%s = %s.%s;\n' % (self.type, struct, self.name)
        return '%s %s = %s.%s;\n' % (self.type, self.name, struct, self.name)

    # const qualified inputs / array inputs should not be restored. Consts for obvious reasons and array
    # decays to pointer type.
    def restore(self, struct):
        if self.isconstq: return '' 
        if self.isarrayt: return ''
        return '%s = %s.%s;\n' % (self.name, struct, self.name)

    def store(self, struct):
        if not self.isoutput and self.isconstq: return ''
        if not self.isoutput and self.isarrayt: return ''
        return '%s.%s = %s;\n' % (struct, self.name, self.name)

    # read xml into variable type.
    @staticmethod
    def create(xml):
        name = xml.find('name')
        type = xml.find('type')
        isoutput = xml.find('isoutput')
        isfunptr = xml.find('isfunptr')
        isstatic = xml.find('isstatic')
        isconstq = xml.find('isconstq')
        isarrayt = xml.find('isarrayt')
        
        #name, ptrl and type are required
        if name == None or type == None:
            raise Exception('Missing variable info');
        
        variable = Variable(name.text, type.text.strip())
        if isoutput != None: variable.isoutput = bool(isoutput.text)
        if isfunptr != None: variable.isfunptr = bool(isfunptr.text)
        if isstatic != None: variable.isstatic = bool(isstatic.text)
        if isconstq != None: variable.isconstq = bool(isconstq.text)
        if isarrayt != None: variable.isarrayt = bool(isarrayt.text)
        return variable

# if condition class for possible return / goto statements inside the region that we have to check 
# in the caller function
class RegionExit:
    STMT_GOTO    = 0
    STMT_RET     = 1
    STMT_RETVOID = 2

    def __init__(self, flg, val, storevar, stmt):
        self.flgvar = flg #flag variable. If we return/goto we set this flag to 1 
        self.valvar = val #if we return some value, we store that value in this variable
        self.storevar = storevar  #variable to store in variable above
        self.stmt = stmt
        self.flgvar.isoutput = True

    # we don't need to add return value if we either return void or we go to some label.
    def as_struct_member(self):
        if self.stmt == RegionExit.STMT_GOTO:    return self.flgvar.as_struct_member()
        if self.stmt == RegionExit.STMT_RETVOID: return self.flgvar.as_struct_member()
        return self.flgvar.as_struct_member() + self.valvar.as_struct_member()

    # returns the if statement that checks if condition flag is set and then adds return / goto 
    # statements accordingly.
    def make_conditional_stmt(self, struct):
        val = ''
        if self.stmt == RegionExit.STMT_GOTO:    val = 'goto %s;' % self.storevar.name
        if self.stmt == RegionExit.STMT_RET :    val = 'return %s.%s;' % (struct, self.valvar.name)
        if self.stmt == RegionExit.STMT_RETVOID: val = 'return;'
        return 'if (%s.%s) { %s }\n' % (struct, self.flgvar.name, val)

    # declares flags inside the region and sets them to 0
    def initialize(self, struct):
        return '%s.%s = 0;\n' % (struct, self.flgvar.name) 

    # store flag and retval into return struct.
    def store(self, struct):
        val = ''
        if self.stmt == RegionExit.STMT_RET: val = '%s.%s = %s;\n' % (struct, self.valvar.name, self.storevar.name) 
        return '%s.%s = 1;\n%s' % (struct, self.flgvar.name, val)

class Function:
    def __init__(self, funname, funrettype):
        self.inputs  = []
        self.outputs = []
        self.special = [] # for return / gotos within region. (see below)

        self.funname    = funname     # name of the extracted function
        self.funrettype = funrettype  # return type of the original function
        self.retvalname = '%s_retval' # name of the structure returned from the extracted function 
        self.retvaltype = 'struct %s_struct' # type name of the structure returned from extracted function

        # if extracted function contains return / goto statements, we need to also return same values
        # from caller function. The idea is to use (flag, value) pairs. After extracted function returns, 
        # we check these flags in the caller and return accordingly.
        self.exitflagname  = '%s_flag_loc%s' 
        self.exitvaluename = '%s_value_loc%s'

    ## add variable to either input / output list.
    def add_variable(self, var):
        if var.isoutput: self.outputs.append(var)
        else: self.inputs.append(var)

    # get extracted function's return type.
    def get_self_return_type(self, toplevel):
        if toplevel: return self.funrettype
        return self.retvaltype % (self.funname)

    def get_self_retval_name(self):
        return self.retvalname % (self.funname)

    # replace return / goto statement in the region with setting flag to 1 and setting value to whatever
    # comes on the rhs of the return statement.
    # we expect return/goto statements formatted in a certain way.
    def check_exit_loc(self, regloc, loc):
        temp = regloc[loc]
        temp = temp.lstrip('\t ')
        temp = temp.rstrip('\n; ')
        rett = self.get_self_retval_name()


        retstmt = line_contains(temp, 'return')
        if retstmt != (None, None):
            storeinto, storetarget, stmttype = None, None, RegionExit.STMT_RETVOID
            flg = Variable(self.exitflagname  % (self.funname, loc), 'char')
            if retstmt[1] != None: 
                storetarget = Variable(retstmt[1], self.funrettype) 
                storeinto   = Variable(self.exitvaluename % (self.funname, loc), self.funrettype) 
                stmttype = RegionExit.STMT_RET

            exit = RegionExit(flg, storeinto, storetarget, stmttype)
            self.special.append(exit) 
            regloc[loc] = '%sreturn %s;\n' % (exit.store(rett), rett)    
            return

        ## TODO 
        gotostmt = line_contains(temp, 'goto')
        if gotostmt != (None, None):
            storeinto, storetarget, stmttype = None, None, RegionExit.STMT_RETVOID
            flg = Variable(self.exitflagname  % (self.funname, loc), 'char')
            storeinto   = Variable(self.exitvaluename % (self.funname, loc), self.funrettype) 
            storetarget = Variable(gotostmt[1], self.funrettype) 
            stmttype = RegionExit.STMT_GOTO 
            exit = RegionExit(flg, storeinto, storetarget, stmttype)
            self.special.append(exit) 
            regloc[loc] = '%s%sreturn %s;\n' % (exit.store(rett), self.store_retvals_and_return(False), rett)    

    ## returns function definition.
    def get_fn_definition(self, toplevel):
        args = '' 
        for var in self.inputs: args = args + var.as_function_argument() + ', '
        args = args.rstrip(', ') 
        return ('%s %s(%s) {\n') % (self.get_self_return_type(toplevel), self.funname, args)

    # returns correct function call string
    # if the region is toplevel, we do not need to return a structure from the extracted function, 
    # and just returning same type as original function would be sufficient.
    def get_fn_call(self, toplevel):
        args = ''
        for var in self.inputs: args = args + var.name + ', '
        args = args.rstrip(', ') 

        rett = self.get_self_return_type(toplevel)
        retn = self.get_self_retval_name()
        if toplevel and rett == 'void': return '\t%s(%s);\n' % (self.funname, args)
        if toplevel and rett != 'void': return '\treturn %s(%s);\n' % (self.funname, args)
        return '%s %s = %s(%s);\n' % (rett, retn, self.funname, args)

    # Defines a structure that is returned from extracted function.
    # If region is toplevel, we do not need such structure - return value directly.
    # Const qualified inputs should not be stored in return structure.
    def declare_return_type(self, toplevel):
        if toplevel: return ''

        args = '' 
        for var in self.inputs:  args = args + var.as_struct_member() 
        for var in self.outputs: args = args + var.as_struct_member()
        for var in self.special: args = args + var.as_struct_member()
        type = self.retvaltype % (self.funname)
        return '%s {\n%s};\n\n' % (type, args)

    # Defines return value in the beginning of the extracted function and sets 
    # special return flags to 0 if those exist
    # If region is toplevel, we do not need this.
    def define_return_value(self, toplevel):
        if toplevel: return ''

        name = self.retvalname % (self.funname)
        type = self.retvaltype % (self.funname)
        out  = '\t%s %s;\n' % (type, name)
        for var in self.special: out = out + var.initialize(self.get_self_retval_name())
        return out 


    # Stores all local variable in return structure before exiting extracted function.
    # Regions with return statements are handled somewhere else...
    # If region is toplevel, we do not need this!
    # const-qualified inputs should not be stored!
    def store_retvals_and_return(self, toplevel):
        if toplevel: return ''

        args = ''
        retn = self.get_self_retval_name()
        for var in self.inputs:  args = args + var.store(retn)
        for var in self.outputs: args = args + var.store(retn)
        return '%sreturn %s;\n' % (args, retn)

    # Restores local variables in the caller from the structure returned by
    # extracted function, definining it if necessary. 
    # If region is toplevel, we do not do this!
    # const-qualified inputs should not be restored!
    def restore_retvals(self, toplevel):
        if toplevel: return ''

        args = ''
        retn = self.get_self_retval_name()
        for var in self.inputs:  args = args + var.restore(retn)
        for var in self.outputs: args = args + var.declare_and_initialize(retn)
        for var in self.special: args = args + var.make_conditional_stmt(retn)
        return args 
        
# returns the string if it exists in the string. Also returns everything on the right-hand side of such 
# string. Useful for goto/return statements. Performs certain assertion checks to ensure the code extracted
# is formatted appropriately. See above for formatting details.
def line_contains(line, string):
    idx = line.find(string) 
    if idx != -1:
        if (len(string) == len(line)): return (string, None) # just have return statement, nothing on rhs.
        lhs = line[:(idx+len(string))].strip(' \n')
        rhs = line[(idx+len(string)):].strip(' \n;')
        lhstemp = strip_char_string_literals(lhs)
        rhstemp = strip_char_string_literals(rhs)
        # ensure there are no letters around the word. 
        if (line[idx + len(string)].isalnum()): return (None, None)
        if not (idx == 0 or not line[idx-1].isalnum()): return (None, None)

        assert(rhstemp.find('}') == -1 and lhstemp.find('{') == -1)
        assert(rhstemp.find(';') == -1) # must not have anything after return statement
        assert(lhstemp == string) # must not have anything before the word. 
        return (string, rhs) 
    return (None, None)

# counts number of opening / closing braces. Expects loc to be a dictionary of strings.
def brace_count(loc):
    numopeningbraces = 0
    numclosingbraces = 0
    for line in loc.values():
        line = strip_char_string_literals(line); 
        numopeningbraces = numopeningbraces + line.count('{')
        numclosingbraces = numclosingbraces + line.count('}')
    return (numopeningbraces, numclosingbraces)

# remove all literal chars / literal strings from the line.
def strip_char_string_literals(line):
    ret = re.sub(r'\'(.)*\'', '', line, re.M | re.I)
    ret = re.sub(r'\"(\\.|[^"])*\"', '', ret, re.M | re.I) 
    return ret

#Boring parsing stuff
# Read XML file.
def parse_xml(fileinfo):
    tree = ET.parse(CLI_ARGS.xml);
    for child in tree.getroot():
        if (child.tag == 'funcname'):   fileinfo.funname = child.text
        if (child.tag == 'funcreturntype'): fileinfo.funrettype = child.text
        if (child.tag == 'regionexit'): fileinfo.exitlocs.append(int(child.text))
        if (child.tag == 'region'):     fileinfo.reginfo = LocInfo.create(child)
        if (child.tag == 'function'):   fileinfo.funinfo = LocInfo.create(child)
        if (child.tag == 'variable'):   fileinfo.vars.append(Variable.create(child))
        if (child.tag == 'toplevel'):   fileinfo.toplevel = bool(int(child.text))

# Read original source file into two different dictionaries.
def parse_src(fileinfo):
    f = open(CLI_ARGS.src)
    line = f.readline()
    linenum = 1
    while line != '':
        if CLI_ARGS.append == True and linenum < fileinfo.funinfo.start: 
            fileinfo.prefunc.append(line)
        if CLI_ARGS.append == True and linenum > fileinfo.funinfo.end:
            fileinfo.postfunc.append(line)
        if fileinfo.funinfo.between(linenum):
            if fileinfo.reginfo.between(linenum): fileinfo.regloc[linenum] = line
            else: fileinfo.funloc[linenum] = line
        line = f.readline()
        linenum = linenum + 1
    f.close()

def main():
    fileinfo = FileInfo()
    parse_xml(fileinfo)
    parse_src(fileinfo)

    fileinfo.try_separate_func_header()
    fileinfo.region_find_closing_brace()
    fileinfo.function_add_closing_brace()
    fileinfo.extract()

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--src', help='Source code to extract region from',required=True)
    parser.add_argument('--xml', help='XML file with region info', required=True)
    parser.add_argument('--append', action='store_true', help='Append the rest of file to the output')
    CLI_ARGS = parser.parse_args()
    main()
