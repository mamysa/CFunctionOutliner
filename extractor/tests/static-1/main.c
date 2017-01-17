int main() {
	static int a = 12;

	if (a == 12) { a += 1; }
	else { a += 2; }

	int filler = 0;
	static int b = 12;
	if (b == 15) { a += 1; }
	else { a += 2; }
	
	return a += b;
}
