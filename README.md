# FuncExtractor
TODO - description.

# Getting Started
## Building
* Clone the repo and copy `FuncExtract` directory to `$(ROOT)/llvm/lib/Trasforms`.
* In `Transforms` directory, append `add_subdirectory(FuncExtract)` to `CMakeLists.txt` file.
* Follow [LLVM building guide](http://llvm.org/docs/GettingStarted.html).

# Limitations / General Considerations
 
At this moment this utility is fairly limited in what it can do. 

## Accessing local variables

Since variables are passed into extracted region by value, you have to be extremely careful when using address-of operator and doing pointer arithmetic on local variables. Consider testcase `tests/bad_cases/main.c:test3`. Once region is extracted and variables `a` and `b` are passed into the function, they now have completely different addresses. Futhermore, once extracted function returns, `return *x` in caller now points to invalid address. 

For obvious reasons such error won't generate compiler error. You have to fix such things manually. 

## Array-to-Pointer Decay
Due to array decay, passing array into the function is the same as passing pointer into the function. Thus, if `sizeof` operator will produce different results when used arrays. 

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


## Code Extractor Formatting
TODO
