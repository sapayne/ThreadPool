#include <iostream>
#include <vector>
#include <chrono>
#include <string>

#include "Threadpool.h"

int exampleFunc(std::string inputString, int i)
{
	std::cout << inputString;
	return i;
}

int main()
{
	
	Threadpool pool(4);
	std::vector< std::future<int> > results;
	std::string input;
	for(int i = 0; i < 8; i++) {
		results.emplace_back(
		//  putting the variables in the [] allows for them to be accessed within the {} for parameter passing
			pool.enqueue([input, i] { return exampleFunc(input, i); })
		);

		input = "Hello World " + std::to_string(i) + "\n";
		//  if you are not storing the results in a vector
		pool.enqueue([input, i] { exampleFunc(input, i); });
	}
	std::cout << std::endl;
	for(auto& result: results)
		std::cout << "Task " << result.get() << " finished.\n";
	std::cout << std::endl;
	
	return 0;
}
