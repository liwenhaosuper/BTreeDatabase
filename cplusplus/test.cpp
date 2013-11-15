

#include<iostream>
#include<string>
#include"Function.h"

using namespace std;

int main()
{
	int a=10;
	double b=10;
	string c="10";
	char d='10';
	char* a1=new char[5];
	int_to_charArr(a,a1);
	int a2;charArr_to_int(a1,a2);
	cout<<a2<<endl;
	double_to_charArr(b,a1);
	double b2;charArr_to_double(a1,b2);
	cout<<b2<<endl;
	str_to_charArr(c,a1);
	string str;charArr_to_str(a1,str);
	cout<<str<<endl;
	char_to_charArr(d,a1);
	char d1;charArr_to_char(a1,d1);
	cout<<d1;

	
}