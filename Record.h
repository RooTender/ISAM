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

	void updateInput(const double& radius, const double& height);
	void updateInput(Record record);
	std::pair<double, double> getInput();
	double getVolume();
};
