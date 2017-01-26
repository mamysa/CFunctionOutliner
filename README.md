# CFuncExtract

CFuncExtract is a set of utilities facilitating extraction of single-entry single-exit regions into separate functions and modification of original source code replacing extracted region with suitable function call. 

For example, given the following program:

```c
int main() {
	int a[4] = { 1, 2, 3, 4 };
	int out = 0;
	
	int i;
	for (i = 0; i < 4; i++) { out += a[i]; }

	return out;
}
```

... and given information about regions that we want to extract:

```
for.cond => for.end
```

... the following will be the result: 

```c
struct extracted_rettype { 
	int i;
	int out;
}

struct extracted_rettype extracted(int i, int out, int (a)[4]) {
	struct extracted_rettype retval;
	for (i = 0; i < 4; i++) { out += a[i]; }

	retval.i = i;
	retval.out = out;
	return retval;
}

int main() {
	int a[4] = { 1, 2, 3, 4 };
	int out = 0;
	
	int i;
	struct extracted_rettype retval = extracted(i, out, a);
	i = retval.i;
	out = retval.out;
	return out;
}
```

# Getting Started
## Building
* Clone the repo and copy `FuncExtract` directory to `$(ROOT)/llvm/lib/Trasforms`.
* In `Transforms` directory, append `add_subdirectory(FuncExtract)` to `CMakeLists.txt` file.
* Follow [LLVM building guide](http://llvm.org/docs/GettingStarted.html).

## Running LLVM Pass
To extract the region, we have to run supplied LLVM pass first. For that, we need to create a text file listing all functions and regions we wish to extract. Once such file is required for each source file you are extracting from.

In the example below, `main` is the name of the function and `for.cond => for.end` is the region.

```
echo "main: for.cond => for.end" >> mysourcefile_regions.txt
```

Next, we have to compile the file that we want to extract from. Note that LLVM pass requires zero optimization (`-O0`) and debug information (`-g`) flags to be present! The command below creates `mysourcefile.ll` file.

```
clang -emit-llvm -O0 -g -S mysourcefile.c
```

Now we are ready to run the LLVM pass. LLVM pass takes a number of arguments:
* `--bblist` - text file listing regions we have created above.
* `--out` - directory to which output of the pass will be written. Useful for when we are extracting multiple regions from a single file.

We can run the pass as follows:
```
opt -load $ROOTDIR/build/lib/FuncExtract.so -funcextract --bblist=regions.txt --out=outdir/ mysourcefile.ll 
```

After running the pass a number of XML files can be found in the output directory, one file for each region.

##Running Extractor Script
Code extractor (`extractor/extractor.py`) also takes a number of arguments:

* `--src` - source code we are extracting from (i.e. `mysourcefile.c`).
* `--xml` - XML file that LLVM pass outputs.
* `--append` - includes the rest of the `mysourcefile.c` along with extracted function.

We can run script as follows:

```
python extractor.py --src mysourcefile.c  --xml myfunc_forcond_forend.xml  --append > extracted.c
```

# Structure of LLVM Pass Output
LLVM pass outputs XML file with a number of properties.

* `region` indicates at which line region starts and at which it ends. `function` does the same but for functions.
* `regionexit` contains line numbers where control flow goes outside the region. We need this to determine if there are `return` or `goto` statements inside the region.
* `funcreturntype` is a return type of the function. 
* `funcname` is the name of the extracted functions. Defaults to the label of the region.
* `toplevel` is a boolean indicating if the region is top level (i.e. it spans the entire function). 
* `variable` contains information about inputs / outputs of the region.
	* `name` - name of the variable.
	* `type` - C type of the variable.
	* `typehasname` - boolean indicating if the name of the variable is already included into type. This is required for arrays and function pointers.
	* `isoutput` - `1` if variable is declared inside the region and used outside the region, `0` otherwise.
	* `isfunptr` - `1` if variable is function pointer, `0` otherwise.
	* `isstatic` - `1` if variable is static, `0` otherwise.
	* `isconstq` - `1` if variable is `const` qualified, `0` otherwise.
	* `isarrayt` - `1` if variable is array, `0` otherwise.

# Limitations / General Considerations
 
At this moment this utility is fairly limited in what it can do. 

## Accessing local variables

Since variables are passed into extracted region by value, you have to be extremely careful when using address-of operator and doing pointer arithmetic on local variables. Consider testcase `tests/bad_cases/main.c:test3`. Once region is extracted and variables `a` and `b` are passed into the function, they now have completely different addresses. Futhermore, once extracted function returns, `return *x` in caller now points to invalid address. 

For obvious reasons such error won't generate compiler error. You have to fix such things manually. 

## Array-to-Pointer Decay
Due to array decay, passing array into the function is the same as passing pointer into the function. Thus, if `sizeof` operator will produce different results when used on arrays. 

## Function-Local typedefs, structs, unions
Function-local types are unsupported at the moment.  Refer to `tests/bad_cases/main.c:test1`. You have to fix it manually by declaring the type in global scope.

## Incorrect Type Detection
There might be some cases when C types are not being reconstructed properly (since we have to reconstruct them from debug metadata). In this case, you should use `typedef`. 

## For Loops
If your region happens to start with `for` loop, its initializer is actually initialized outside the region. This problem can be fixed in the following manner:

```c
// this could be a problem
for (int i = mystruct.a; i < 10; i++)

// this won't be a problem
int i = mystruct.a;
for (;i < 10; i++)
```

## Const-qualified literals
`clang` propagates all const-qualified basic literals (i.e. `float`, `int`, etc) and thus there are now instructions that use 'magic' numbers. Any variable that is initialized to a literal is detected as an input to the region if there's an instruction inside the region using the literal with the same value. Same logic applies to outputs.

This approach is not precise since if there are multiple variables initialized to the same literal value, they will all be detected as input or output where applicable.

## Extractor Formatting / Troubleshooting
Code extractor requires code to be formatted in a certain way to perform extraction correctly. If there are any `return` or `goto` statements inside the region, they have to be on their own line. Otherwise, assertion will fail.

One must be careful when declaring functions. Declaring function return type on its own line can cause problems (LLVM debug metadata is not that precise) and therefore it is recommended to have both function return type and function name on the same line. You can also manually modify function's starting line number to avoid reformatting code.

```c
// this is bad
int *
my_function(int a) { ...

// this is good
int * my_function(int a) { ...
```

If the script throws an exception, you have to manually modify region's / function's bounds to fix the problem.
