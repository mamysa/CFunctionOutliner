int func1(int a) {
	return 2 *a;
}

int main(void) {
	int (*myfunction)(int) = &func1;
	int out = 0;

	int i;
	for (i = 0; i < 4; i++) {
		out = out + myfunction(i);
	}

	return out;
}
