int main() {
	int a = 12;

	if (a == 12) { a += 1; }
	else { a += 2; }

	int x[4] = {1, 2, 3, 4};
	if (a == 12) {
		x[3] = x[3] * 2;
	}
	else {
		x[3] = x[2] * 2;
	}
	
	return x[3];
}
