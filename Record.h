#pragma once
#include <utility>

class Record
{
private:
	static constexpr double PI = 3.141592653589793;
	std::pair<double, double> input;
	double volume;

	bool deleteFlag;

public:
	Record(std::pair<double, double> input);
	Record(const double& radius, const double& height);

	std::pair<double, double> getInput();
	double getVolume();
};
