int func1(int a) {
	return 2 *a;
}

void func2(int *a) {
	(*a) = 2 * (*a);
}

void func3(void) {
	int a = 12;
}




//expected inputs: myfunction, i, out
int test1(void) {
	int (*myfunction)(int) = &func1;
	int out = 0;

	int i;
	for (i = 0; i < 4; i++) {
		out = out + myfunction(i);
	}

	return out;
}

//expected inputs: myfunction, i, out
int test2(void) {
	void (*myfunction)(int *) = &func2;
	int out = 0;

	int i;
	for (i = 0; i < 4; i++) {
		int b = i;
		myfunction(&b);
		out = out + b;
	}

	return out;
}

//expected inputs: myfunction, i, out
int test3(void) {
	void (*myfunction)(void)= &func3;
	int out = 0;

	int i;
	for (i = 0; i < 4; i++) {
		myfunction();
		out = out + i;
	}

	return out;
}

typedef void(*myfunction_t)(int *); 
int test4(void) {
	myfunction_t myfunc = &func2;
	int out = 0;

	int i;
	for (i = 0; i < 4; i++) {
		int b = i;
		myfunc(&b);
		out = out + b;
	}

	return out;
}

// testing const function pointers...
int test5(void) {
	void (*const myfunction)(int *) = &func2;
	int out = 0;

	int i;
	for (i = 0; i < 4; i++) {
		int b = i;
		myfunction(&b);
		out = out + b;
	}

	return out;
}
