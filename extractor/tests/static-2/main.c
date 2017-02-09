int main() {
	static int a = 12;

	if (a == 12) { a += 1; }
	else { a += 2; }

	int filler = 0;
	static int b[2] = { 12, 15 };
	if (b[0] == 15) { a += 1; }
	else { a += 2; }
	
	return a += b[1];
}
