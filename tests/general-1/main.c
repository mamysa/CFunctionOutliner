// Function arguments have to be passed into the region. 
// INPUTS: out, a, b, c
int test1(int a, int b) {
	int c = 45;	
	int out = 0;
//---region start
	for (out = 0; out < c; ) { out = a + b + out; }
//---region end 
	return out;
}

// Also tests function arguments.
// INPUTS: a, b
// OUTPUTS: c
int test2(int a, int b) {
//---region start
	int c = 45;
	if (a + b == c) { c = a + b + c - 1; }
	else { c = a - b; }
//---region end
	c = c + 12;
	return 0;
}

// General-purpose input/output test.
int test3(void) {
	int a = 12;
	int b = 14;
	if (a + b == 20) { a = a + 11; }
	else { b = b + 1; }
//---region start
	int c = 16;
	if (a + c == 20) { a = a + c; }
	else { c = b + c; }
//---region end
	a = a + 1;
	return a;
}

struct mystruct { int x; int y; }; 
// Passing const struct as function argument
// INPUTS: int out, const struct mystruct a.
int test4(const struct mystruct a) {
	int out = 0;
	for (; out < 2000;) {
		out += a.x;	
	}

	return out;
}

// INPUTS: const struct mystruct a.
int test5(const struct mystruct a) {
	if (a.x == 10) { int dosomething = 12; }
	else { int dosomething = 15; }
	return a.x + 1;
}

// Function Args Test: Passing pointer to const 
// INPUTS: int out, const int *a.
int test6(const int *a) {
	int out = 0;
	for (; out < 2000;) {
		out += a[1];
	}

	return out;
}

// Static Variable Test 
void test7(void) {
	static int mystaticvar = 4;

	int i;
	for (i = 0; i < 10; i++) {
		mystaticvar = mystaticvar + 1;
	}

	mystaticvar = mystaticvar * 2;
}

// Static Variable Test: local basic static constant.
// This one is expected to fail. Both static const and simply const values 
// happen to have the same linkage and are identical. 
int test8(void) {
	const static int mystaticconst = 4;
	int i;
	int out = 0;
//--- REGION START
	for (i = 0; i < 10; i++) { out += mystaticconst; }
//--- REGION END 
	out = out * 2;
	return out;
}

// Static Variable Test
// global static primitive variable. Should not be detected.
const static int global_static_const = 4;
int test9(void) {
	int i;
	int out = 0;
//--- REGION START
	for (i = 0; i < 10; i++) { out += global_static_const; }
//--- REGION END 
	out = out * 2;
	return out;
}

// Switch case test
enum myenum { ONE, TWO, THREE };
int test10() {
	enum myenum a = ONE;
	int out = 0;

	switch (a) {
		case ONE:   { out = 1; break; }
		case TWO:   { out = 2; break; }
		case THREE: { out = 3; break; }
		default: { out = 13; }
	}

	return out;
}

// Switch case test that is not initialized in entry basic block.
int test11() {
	enum myenum a = ONE;
	int out = 0;
	int i;
	for (i = 0; i < 5; i++) { out += i; }
//--REGION start
	switch (a) {
		case ONE:   { out = 1; break; }
		case TWO:   { out = 2; break; }
		case THREE: { out = 3; break; }
		default: { out = 13; }
	}
//--REGION END
	return out;
}
