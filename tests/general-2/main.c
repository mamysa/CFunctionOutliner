#include <stdlib.h>

enum MyEnum { ONE, TWO };
// Testing enums...
// INPUTS: int i, int out, enum MyEnum n
int test1(void) {
	enum MyEnum n = TWO;	
	int out = 0;
	int i;
//---region start
	for (i = 0; i < 4; i++) {
		if (n == ONE) {
			n = TWO;
			out = out + 12;
		}

		if (n == TWO) {
			n = ONE;
			out = out + 1;
		}
	}
//---region end 
	return out;
}

// same as above but with pointers.
// INPUTS: int i, int out, enum MyEnum *nptr
int test1s1(void) {
	enum MyEnum n = TWO;	
	enum MyEnum *nptr = &n;
	int out = 0;
	int i;
//---region start
	for (i = 0; i < 4; i++) {
		if (*nptr == ONE) {
			*nptr = TWO;
			out = out + 12;
		}

		if (*nptr == TWO) {
			*nptr = ONE;
			out = out + 1;
		}
	}
//---region end 
	return out;
}

union myunion {int a; float b; };
typedef union myunion myunion_t;
// testing unions...
// INPUTS: int out, union myunion x
int test2(void) {
	union myunion x;
	x.a = 12;

	int out = 0;
	for (; out < 1200;) { out += x.a; }
	return out;
}

// INPUTS: int out, union myunion *xptr
int test2s1(void) {
	union myunion *xptr = (union myunion *)malloc(sizeof(union myunion));
	xptr->a = 12;

	int out = 0;
	for (; out < 1200;) { out += xptr->a; }
	
	free(xptr);
	return out;
}

// testing typedefs...
int test3(void) {
	myunion_t x;
	x.a = 12;

	int out = 0;
	for (; out < 1200;) { out += x.a; }
	return out;
}


int test4(void) {
	const myunion_t m = { 12 };
	const myunion_t * const myunion = &m;

	int out = 0;
	for (; out < 1200;) { out += myunion->a; }
	return out;
}
