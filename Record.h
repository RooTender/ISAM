#pragma once
#include <utility>

class Record
{
	static constexpr double pi = 3.141592653589793;
	std::pair<double, double> input;
	double volume;

	bool deleteFlag = false; // NOLINT(clang-diagnostic-unused-private-field)

public:
	explicit Record(std::pair<double, double> input);
	Record(const double& radius, const double& height);

	void UpdateInput(const double& radius, const double& height);
	void UpdateInput(Record record);
	std::pair<double, double> GetInput();
	double GetVolume() const;
};
