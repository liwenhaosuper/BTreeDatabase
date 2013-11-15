

#ifndef FUNCTION_H
#define FUNCTION_H

#include<iostream>
#include<string>

using namespace std;

//int to char*
void int_to_charArr(int& intput,char* output)
{
   output=itoa(intput,output,10);
}

//char* to int
void charArr_to_int(char* input,int& output)
{
	output=atoi(input);
}

//double to char*
void double_to_charArr(double& input,char* output)
{
	gcvt(input,5,output);
    
}

//char* to double
void charArr_to_double(char* input,double& output)
{
	output=atof(input);
}

//string to char*
void str_to_charArr(string& input,char* output)
{
	strcpy(output,input.c_str());
}
//char* to string
void charArr_to_str(char* input,string& output)
{
	string str(input);
	output=str;
}

//char to char*
void char_to_charArr(char& input,char* output)
{
	output=new char[1];
	output[0]=input;
}

//char* to char
void charArr_to_char(char* input,char& output)
{
	output=input[0];
}




#endif