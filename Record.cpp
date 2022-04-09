#include "Record.h"

Record::Record(const std::pair<double, double> input) : input(input),
                                                        volume(input.first * input.first * pi * input.second)
{
}

Record::Record(const double& radius, const double& height) : Record(std::pair<double, double>{radius, height})
{
}

void Record::UpdateInput(const double& radius, const double& height)
{
	this->volume = radius * radius * pi * height;
}

void Record::UpdateInput(Record record)
{
	this->input = record.GetInput();
	this->volume = record.GetVolume();
}

std::pair<double, double> Record::GetInput()
{
	return this->input;
}

double Record::GetVolume() const
{
	return this->volume;
}
