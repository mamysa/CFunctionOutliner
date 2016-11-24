import sys

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

    print(fileInfo)
    return fileInfo 



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
        if line.find('FileInfo{') != -1: parseFileInfo(f)
        if line.find('Variable{') != -1: parseVariable(f)
         

        line = f.readline()


main()
