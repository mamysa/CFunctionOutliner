struct mystruct {int a; int b; };


//pointer to non-const integer.
// inputs: i, out, xptr
int test1(void) {
	int x = 12;
	int *xptr = &x;

	int out = 0;
	int i;
	for (i = 0; i < 4; i++) { out = out + *xptr; }

	return out;
}

// pointer to const integer.
// inputs: out, i, xptr
int test2(void) {
	const int x = 12;
	const int *xptr = &x;

	int out = 0;
	int i;
	for (i = 0; i < 4; i++) { out = out + *xptr; }

	return out;
}

// pointer to non-constant struct...
// inputs: i, out, xptr
int test3(void) {
	struct mystruct x = { 13, 14 };
	struct mystruct *xptr = &x;

	int out = 0;
	int i;
	for (i = 0; i < 4; i++) { out = out + xptr->a; }


	return out;
}

// pointer to const struct
// inputs: i, out, xptr
int test4(void) {
	const struct mystruct x = { 13, 14 };
	const struct mystruct *xptr = &x;

	int out = 0;
	int i;
	for (i = 0; i < 4; i++) { out = out + xptr->a; }


	return out;
}

// testing void pointers. 
// inputs: i, out, u, v;
int test5(void *u, const void *v) {
	int out = 0;
	int i;

	for (i = 0; i < 4; i++) { 
		struct mystruct *x = (struct mystruct *)u;
		const struct mystruct *y = (const struct mystruct *)v;
		out -= x->b + y->b; 
	}

	return out;
}

// const pointer to int
int test6(void) {
	int a = 12;
	int * const aptr = &a;

	int out = 0;
	int i;
//---REGION START
	for (i = 0; i < 4; i++) { 
		out += *aptr;
	}
//---REGION END
	return out;
}
