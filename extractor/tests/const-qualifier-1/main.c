int main() {
	int a = 12;
	int b = 14;
	const int aconst = a; 

	int out = 0;
	if (aconst == 12) { out += 1; }
	else { out += 2; }

	const int bconst = b;	
	if (bconst == 15) { out += 1; }
	else { out += aconst; }
	
	return out += bconst;
}
