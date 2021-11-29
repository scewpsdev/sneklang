import Console;


struct TestStruct {
	int i;
	float f;
}

class TestClass {
	
}

/*
class TestClass {
	int i;
	float f;

	TestClass() {
		printf("Constructor\n".buffer);
	}

	float foo(int a, int b, int c) {
		return 0.1;
	}
}
*/


int strcmp(char* str1, char* st2);



void runTests(int a, int b, int c) {
	int n = 5;
	int sum = 0;
	int i = 0;
	while ++i <= n {
		sum += i;
		if i == 4 break;
	}
	
	printf("sum = %d\n".buffer, sum);
	TestClass clss = new TestClass();
	
	TestStruct test;
	test.i = 5;
	test.f = 1.23456789;
	printf("%d, %.9f\n".buffer, test.i, test.f);
	
	
	/*
		TestClass clss = null;
		clss = new TestClass();
		clss.i = 10;
		clss.f = 0.987654321;
		float abc = clss.foo(1, 2, 3);
		free clss;
		printf("%.2f\n".buffer, abc);
		*/
	/*
	printf("%d, %.6f\n".buffer, clss.i, clss.f);
	*/
}