int func1(int a) {
	return 2 *a;
}

void func2(int *a) {
	(*a) = 2 * (*a);
}

void func3(void) {
	int a = 12;
}

void func4(int a[2]) {
	a[0] *= 2;
	a[1] *= 2;
}

void func5(int a[2][3], int j) {
	a[j][0] *= 2;
	a[j][1] *= 2;
	a[j][2] *= 2;
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

// function pointer with array argument...
int test6(void) {
	void (*myfunction)(int[2]) = func4;
	int arr[2] = { 2, 4 };
	int i;
	for (i = 0; i < 4; i++) {
		myfunction(arr);
	}

	return arr[0];
}

int test7(void) {
	void (*myfunction)(int[2][3], int) = func5;
	int arr[2][3] = { 
		{ 2, 4, 6 },
		{ 3, 6, 9 },
	};

	int i;
	for (i = 0; i < 2; i++) {
		myfunction(arr, i);
	}

	return arr[0][0];
}
