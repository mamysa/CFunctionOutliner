// Set of functions testing const qualified basic local variables such as ints / floats.
// The story with these is that clang propagates such values to instructions if these
// constants are initialized with literal. 
// This really shouldn't be a problem in C though, as most of the time you'd be using 
// #define directive for such things. 

int test1(void) {
	int a = 0;
	const int myconstant_1 = 1000;
	const int myconstant_2 = 2000;
	const int myconstant_3 = 1000;

	float b = 0.0f;
	const float myfloatconst_1 = 130.f;
	const float myfloatconst_2 = 140.f;
	const float myfloatconst_3 = 150.f;

	int i;
// region start
// inputs: myconstant_1, myconstant_2, myconstant_3,
// myfloatconst_1 myfloatconst_2, i, a, b
	for (i = 0; i < 4; i++) {
		a = a + myconstant_1 + myconstant_2; 
		b = b + myfloatconst_1 + myfloatconst_2; 
	}
//region end
	
	b = myfloatconst_2 + myfloatconst_3;
	return a + (int)b; 
}

// expect myconstant_1, myconstant_2, myconstant_3, out to be inputs.
// Because of const propagation,  'myconstant_2 + myconstant_4' is already precomputed.
// EXPECTED TO FAIL;
int test2(void) {
	int out = 0;
	const int myconstant_1 = 1000;
	const int myconstant_2 = 2000;
	const int myconstant_3 = 1000;


	if (out == 30) { out += myconstant_3; }
	else { out += myconstant_2; }

//---region start
	const int myconstant_4 = 1000;
	if (out == 20) { out += myconstant_1; }
	else { out += myconstant_2 + myconstant_4; }
//---region end 

	return out;
}

//inputs:  myconstant_1, myconstant_2, out
//outputs: myconstant_3, myconstant_4
int test3(void) {
	int out = 0;
	const int myconstant_1 = 1000;
	const int myconstant_2 = 2000;

	if (out == 30) { out += myconstant_1; }
	else { out += myconstant_2; }

	const int myconstant_3 = 1000;
	const int myconstant_4 = 1000;
	if (out == 20) { out += myconstant_1; }
	else { out += myconstant_2; }

	return out + myconstant_4;
}

//constants not initialized to literal value are treated like regular variables
//input: myconstant_1, myconstant_2, out
int test4(void) {
	int a = 12;
	int b = 4;

	int out = 0;
	const int myconstant_1 = a + b;
	const int myconstant_2 = b - a;

	if (out == 30) { out += myconstant_1; }
	else { out += myconstant_2; }

	if (out == 20) { out += myconstant_1; }
	else { out += myconstant_2; }

	return out;
}

// constants that are cast are not not detected. 
// input: myconstant_1, myconstant_2, out
int test5(void) {
	int out = 0;
	const int myconstant_1 = 12; 
	const int myconstant_2 = 14; 

	if (out == 30) { out += myconstant_1; }
	else { out += myconstant_2; }

//region start
	if (out == 20) { out += myconstant_1; }
	else { out += (float)myconstant_2; }
//region end

	return out;
}

// In this case, variables that are only set once and never used later (i.e. 'unused variables')
// may also end up being detected as an input / output. Variable 'a' here will be detected as an
// input into the region. 
// INPUTS: a, myconstant_1 i, out
int test6(void) {
	int a = 4;
	const int myconstant_1 = 4;

	int i; 
	int out = 0;
//-----region start
	for (i = 0; i < 8; i++) { out += myconstant_1; }
//-----region end 

	return out;
}

// if local basic const variable and global const variable happen to share the same literal value, 
// local constant is going to be detected as input.
// INPUTS myconstant_1, i, out;
const int myglobalconstant = 44;
int test7(void) {
	const int myconstant_1 = 44;

	int i; 
	int out = 0;
//-----region start
	for (i = 0; i < 8; i++) { out += myglobalconstant; }
//-----region end 

	return out;
}

// finally a bunch of weird cases that involve consts and array / struct access. 
// INPUTS: i, a, myarrayidx, out;
// EXPECTED TO FAIL, since when accessing array elements GEP instruction uses i64 instead
// of i32. If we were to change type of myarrayidx to long, it would pass.
int test8(void) {
	const int myarrayidx = 14;
	int a[16] = { 5 };
	int out = 0;
	int i;

//-----region start
	for (i = 0; i < 8; i++) { out += a[myarrayidx]; }
//-----region end 

	return out;
}

//INPUTS: i, a, out
//EXPECTED TO FAIL, accessing the third structure element 'z' involves GEP i32 2. We 
//happen to have myarrayidx2 that is also initialized to 2, so myarrayidx2 is detected
//as an input whereas it shouldn't.
struct mystruct { int x; int y; int z; };
int test8s1(void) {
	const int myarrayidx2 = 2;
	struct mystruct a = { 1, 3, 6 };
	int out = 0;
	int i;
//-----region start
	for (i = 0; i < 8; i++) { out += a.z; }
//-----region end 

	return out;
}
