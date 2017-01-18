int test1(void) {
	const int a[2] = {1, 2};
	const int b[2] = {4, 6};
	int out = 0;

	if (a[0] + b[0] == 20) { out += a[0] + 11; }
	else { out += b[1] + 1; }

	const int c[2] = {4, 6}; // this one is outside the region.
	if (a[0] + c[1] == 20) { out += c[0] + a[1]; }
	else { out += c[0] + b[0]; }

	return out;
}

// same as above but with literal structs
struct mystruct { int x; int y; };
int test1s1(void) {
	const struct mystruct a[1] = { {1, 2} };
	const struct mystruct b[1] = { {4, 6} };
	int out = 0;

	if (a[0].x + b[0].y == 20) { out += a[0].x + 11; }
	else { out += b[1].y + 1; }

	const struct mystruct c[2] = { {4, 6} } ; // this one is outside the region.
	if (a[0].x + c[1].x == 20) { out += c[0].x + a[1].y; }
	else { out += c[0].y + b[0].x; }

	return out;
}

int test1s2(void) {
	struct mystruct i = {1, 2};
	struct mystruct j = {4, 6};
	struct mystruct k = {4, 6};
	const struct mystruct * const a[1] = { &i };
	const struct mystruct * const b[1] = { &j };
	int out = 0;

	if (a[0]->x + b[0]->y == 20) { out += a[0]->x + 11; }
	else { out += b[1]->y + 1; }

	const struct mystruct * const c[2] = { &k } ; // this one is outside the region.
	if (a[0]->x + c[1]->x == 20) { out += c[0]->x + a[1]->y; }
	else { out += c[0]->y + b[0]->x; }

	return out;
}

int test2(void) {
	const int a[2] = {1, 2};
	const int b[2] = {4, 6};
	int out = 0;

	if (a[0] + b[0] == 20) { out += a[1] + 11; }
	else { out += b[0] + 1; }

	// c is now _inside_ the region as d is defined before it. 
	int d = 12;
	const int c[2] = {4, 6}; 
	if (a[1] + c[0] == 20) { out += c[1] + a[0]; }
	else { out += c[0] + b[1] + d; }

	return out;
}

// these tests below are identical to struct tests. Arrays behave in the 
// same way as structures.
// INPUTS: a, b, out, c
// c is defined outside the region
int test3(void) {
	const int a[2] = {1, 2};
	const int b[2] = {4, 6};
	int out = 0;

	if (a[0] + b[0] == 20) { out += a[0] + 11; }
	else { out += b[0] + 1; }

	const int c[2] = {1, 2};
	int d = 12;
	if (a[0] + c[0] == 20) { out += c[0] + a[0]; }
	else { out += c[0] + b[0] + d; }

	return out;
}

// c is now inside the region and it's an output
int test4(void) {
	int a[2] = {1, 2};
	int b[2] = {4, 6};

	int out = 0;

	if (a[0] + b[0] == 20) { out += a[0] + 11; }
	else { out += b[0] + 1; }

	// c is now _inside_ the region as d is defined before it. 
	int d = 12;

	int c[2] = {4, 6};
	if (a[0] + c[0] == 20) { out += c[0] + a[0]; }
	else { out += c[0] + b[0] + d; }

	return out + c[0];
}
