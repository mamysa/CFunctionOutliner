int main() {
	int a = 12;

	if (a == 12) { a += 1; }
	else { a += 2; }

	int b[2][3] = {
		{ 1, 2, 3 }, 
		{ 5, 6, 1 }
	};

	int const c[2][3] = { 
		{ 3, 4, 5 },
		{ 1, 9, 2 },
	};

	if (a == 12) {
		b[1][0] = b[1][0] * 2;
	}
	else {
		b[2][0] = b[2][0] * 2;
	}

	int x, y;
	int sum = 0;
	for (y = 0; y < 2; y++)
	for (x = 0; x < 3; x++) {
		sum += b[y][x]; 
	}

	return sum + c[0][1]; 
}
