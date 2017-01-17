int main() {
	int a = 12;
	if (a == 13) { 
		a = a + 13;
		goto myreturnlabel;
	}
	else {
		a = a - 2; 
		goto myreturnlabel;
	}

myreturnlabel:
	return a;
}
