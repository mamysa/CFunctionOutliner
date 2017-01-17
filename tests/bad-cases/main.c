// Function-local structures are unsupported.
int test1() {
	struct {int x; float y; } mystruct = {1, 5.6};
	int i;
	int out = 0;
	for (i = 0; i < 12; i++) { out = out + mystruct.x; }
	return out;
}

struct globalstruct { int x; int y; };
// for loop initializer is initialized outside the region.  
int test2() {
	struct globalstruct mystruct = { 0, 15 };
	int i;
	int out = 0;
	for (i = mystruct.x; i < 12; i++) { out = out + 2; }
	return out;
}

// This one will most likely segfault. Once region is extracted, 
// x = &a will be the address of the function argument and not the original a.
int test3() {
	int a = 12;
	int b = 14;
	int i = 0; 
	for (; i < 2; i++) { 
		a = a - 1;
		b = b + 1;
	}

// REGION START
	int *x;
	if (a == 10) {
		x = &a;
	}
	else {
		x = &b;
	}
// REGION END
	return *x;
}
