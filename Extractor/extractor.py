import sys

class RegionInfo:
    def __init__(self, _start, _end):
        self.start = _start
        self.end = _end

    def valid(self):
        return self.start != -1 and self.end != -1

    def __repr__(self):
        return '<RegionInfo start:%s end:%s>' % (self.start, self.end)


class Variable: 
    def __init__(self, _name, _type, _ptrl, _isoutput):
        self.isoutput = _isoutput 
        self.name = _name 
        self.type = _type 
        self.ptrl = _ptrl 

    def valid(self):
        return self.name != "" and self.type != "" and self.ptrl != -1

    def __repr__(self):
        return '<RegionInfo name:%s type:%s ptrl:%s isoutput:%s>' % (self.name, self.type, self.ptrl, self.isoutput)

def sanitize(line):
    line = line.strip(" ")
    line = line.replace(' ' , '')
    line = line.replace('\n', '')
    return line


def parseRegionInfo(f):
    regionInfo = RegionInfo(-1, -1)

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
        if line[0] == 'START': regionInfo.start = int(line[1])
        if line[0] == 'END'  : regionInfo.end   = int(line[1])
        line = f.readline()

    if not regionInfo.valid():
        sys.stdout.write('invalid region info object\n')
        sys.exit(1)

    print(regionInfo)
    return regionInfo



def parseVariable(f):
    variable = Variable("", "", -1, True)

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
        if line[0] == 'bINPUT': variable.isoutput = False 
        if line[0] == 'NAME'  : variable.name = line[1]
        if line[0] == 'TYPE'  : variable.type = line[1]
        if line[0] == 'PTRL'  : variable.ptrl = int(line[1])
        line = f.readline()

    if not variable.valid():
        sys.stdout.write('invalid variable object\n')
        sys.exit(1)

    print(variable)
    return variable


def main():
    variables = []  

    if len(sys.argv) != 2:
        sys.stdout.write("Expected one argument, actual: " + str(len((sys.argv))-1) + "\n")
        sys.exit(1)

    f = open(sys.argv[1])
    line = f.readline()
    while line != '':
        line = sanitize(line)
        if line.find('Region{') != -1: parseRegionInfo(f)
        if line.find('Variable{') != -1: parseVariable(f)
         

        line = f.readline()


main()
