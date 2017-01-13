struct mystruct { int x; int y; };
int test1(void) {
	const struct mystruct a = {1, 2};
	const struct mystruct b = {4, 6};
	int out = 0;

	if (a.x + b.x == 20) { out += a.x + 11; }
	else { out += b.y + 1; }

	const struct mystruct c = {4, 6}; // this one is outside the region.
	if (a.x + c.x == 20) { out += c.x + a.y; }
	else { out += c.x + b.x; }

	return out;
}

int test2(void) {
	const struct mystruct a = {1, 2};
	const struct mystruct b = {4, 6};
	int out = 0;

	if (a.x + b.x == 20) { out += a.x + 11; }
	else { out += b.y + 1; }

	// c is now _inside_ the region as d is defined before it. 
	int d = 12;
	const struct mystruct c = {4, 6}; 
	if (a.x + c.x == 20) { out += c.x + a.y; }
	else { out += c.x + b.x + d; }

	return out;
}

// INPUTS: a, b, out, c
// c is defined outside the region
int test3(void) {
	const struct mystruct a = {1, 2};
	const struct mystruct b = {4, 6};
	int out = 0;

	if (a.x + b.x == 20) { out += a.x + 11; }
	else { out += b.y + 1; }

	const struct mystruct c = {4, 6}; 
	int d = 12;
	if (a.x + c.x == 20) { out += c.x + a.y; }
	else { out += c.x + b.x + d; }

	return out;
}

// INPUTS: a, b, out
// same test as above but since c is not non-const, it is defined _inside_
// the region.
int test3s1(void) {
	struct mystruct a = {1, 2};
	struct mystruct b = {4, 6};
	int out = 0;

	if (a.x + b.x == 20) { out += a.x + 11; }
	else { out += b.y + 1; }

	struct mystruct c = {4, 6}; 
	int d = 12;
	if (a.x + c.x == 20) { out += c.x + a.y; }
	else { out += c.x + b.x + d; }

	return out;
}

// INPUTS:  a, b, out
// OUTPUTS: c
int test3s2(void) {
	struct mystruct a = {1, 2};
	struct mystruct b = {4, 6};
	int out = 0;

	if (a.x + b.x == 20) { out += a.x + 11; }
	else { out += b.y + 1; }

	struct mystruct c = {4, 6}; 
	int d = 12;
	if (a.x + c.x == 20) { out += c.x + a.y; }
	else { out += c.x + b.x + d; }

	return out + c.x;
}

// INPUTS:  a, b, out
// OUTPUTS: c
// same as above but without using initializer list notation.
int test3s3(void) {
	struct mystruct a;
	struct mystruct b;
	a.x = 1; a.y = 2;
	b.x = 4; b.y = 6;

	int out = 0;

	if (a.x + b.x == 20) { out += a.x + 11; }
	else { out += b.y + 1; }

	struct mystruct c; 
	c.x = 4; c.y = 6;
	int d = 12;
	if (a.x + c.x == 20) { out += c.x + a.y; }
	else { out += c.x + b.x + d; }

	return out + c.x;
}

// c is now inside the region and it's an output
int test4(void) {
	const struct mystruct a = {1, 2};
	const struct mystruct b = {4, 6};
	int out = 0;

	if (a.x + b.x == 20) { out += a.x + 11; }
	else { out += b.y + 1; }

	// c is now _inside_ the region as d is defined before it. 
	int d = 12;
	const struct mystruct c = {4, 6}; 
	if (a.x + c.x == 20) { out += c.x + a.y; }
	else { out += c.x + b.x + d; }

	return out + c.x;
}

int test5(void) {
	const struct mystruct a = {1, 2};
	const struct mystruct b = {4, 6};
	struct mystruct c; 
	int out = 0;

	if (a.x + b.x == 20) { out += a.x + 11; }
	else { out += b.y + 1; }


	if (a.x + a.y == 20) {c = (struct mystruct){ 12, 15 }; }
	else { c = (struct mystruct){ 16, 16}; }

	return  c.x;
}

// global structs should not be detected as input/output
struct mystruct myglobalstruct = {8, 10};
int test6(void) {
	int i;
	int out = 0;
//--- REGION START
	for (i = 0; i < 10; i++) { out = myglobalstruct.x + out; }
//--- REGION  END
	return out + myglobalstruct.y;
}




