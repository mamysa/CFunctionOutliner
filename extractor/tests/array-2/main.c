int main() {
	int a[4] = { 1, 2, 3, 4 };
	int out = 0;
	
	int i;
	for (i = 0; i < 4; i++) {
		a[i] = a[i] * 2;
	}

	for (i = 0; i < 4; i++) {
		out += a[i];
	}

	return out;
}
