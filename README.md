# FuncExtractor
TODO - description.

# Getting Started
## Building
* Clone the repo and copy `FuncExtract` directory to `$(ROOT)/llvm/lib/Trasforms`.
* In `Transforms` directory, append `add_subdirectory(FuncExtract)` to `CMakeLists.txt` file.
* Follow [LLVM building guide](http://llvm.org/docs/GettingStarted.html).

## Preparing Files for Extraction and Running
LLVM pass requires two things:
* Text file with list of function names / regions to be extracted. In the example below we want to extract `entry => if.end` region from function `myawesomefunction`.

```
myawesomefunction: entry => if.end
```

* C source code compiled with `-emit-llvm`, `-O0`, and `-g` flags. 

```
clang -emit-llvm -S -O0 -g myfile.c -o myfile.ll'
```
* Finally, we run the pass. In `<output_directory>`, you will find `.xml` files, one for each region in `<region_list>` file.
```
opt -load $(rootdir)/build/lib/FuncExtract.so -funcextract --bblist=<region_list> --out=<output_directory> <llvm_ir_file>
```

## Running Extractor Script.
Extractor script requires two things - the `xml` file produces by LLVM pass, and the original source file. Script is invoked as follows:

```
python extractor.py myfile.xml mysourcefile.c  > extracted.c 
```

## Troubleshooting Extractor Script
Extractor script is susceptible to code formatting - curly braces are required to be used everywhere.

The following code will result in incorrect extraction:

``` 
if (a == 1) 
	return 12;
```

This one won't:
``` 
if (a == 1) {
	return 12;
}
```

Extractor will throw exception in the following case:

``` 
if (a == 1) { return 12; }
```

Again, solution to this would be putting `return` statement on its own line:

``` 
if (a == 1) {
	return 12;
}
```
TLDR: cannot open and close the block on the same line!

If program crashed with `Could not find closing brace exception`, manually tweaking function's starting and ending linenumbers will help. Refer to `<function>` property in the `xml` file.

# Limitations / General Considerations
 
At this moment this utility is fairly limited in what it can do. 

## Accessing local variables.

Since variables are passed into extracted region by value, you have to be extremely careful when using address-of operator and doing pointer arithmetic on local variables. Consider testcase `tests/bad_cases/main.c:test3`. Once region is extracted and variables `a` and `b` are passed into the function, they now have completely different addresses. Futhermore, once extracted function returns, `return *x` in caller now points to invalid address. 

For obvious reasons such error won't generate compiler error. You have to fix such things manually. 

## Function-Local typedefs, structs, unions
Function-local types are unsupported at the moment.  Refer to `tests/bad_cases/main.c:test1`. You have to fix it manually by declaring the type in global scope.

## Incorrect Type Detection
There might be some cases when C types are not being reconstructed properly (since we have to reconstruct them from debug metadata). In this case, you should use `typedef`. 

## For Loops
If your region happens to start with `for` loop, its initializer is actually initialized outside the region. This problem can be fixed easily:

```
// this could be a problem
for (int i = mystruct.a; i < 10; i++)

// this won't be a problem
int i = mystruct.a;
for (;i < 10; i++)
```
