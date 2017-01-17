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


# Frequently Asked Questions

**Q: Why some of my structs/unions are of type struct/union void?**
**A:**   See `tests/bad_cases/main.c:test1`. Function-local types are not supported at this time.

**Q: I get 'could not find closing brace' exception. What do I do?**
**A:** Verify both region's and function's starting/ending line numbers are sensible. LLVM debug metadata is not precise enough, especially when trying to find boundaries of functions and regions. 

**Q: Why does my extracted program segfault?**
**A:** See `tests/bad_cases/main.c:test3` function for one possible culprit. In short, pointer arithmetic on local stack allocated objects is currently unsupported and source code has to be modified manually.
