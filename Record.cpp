#include "Record.h"

Record::Record(const std::pair<double, double> input) : input(input),
                                                        volume(input.first * input.first * pi * input.second)
{
}

Record::Record(const double& radius, const double& height) : Record(std::pair<double, double>{radius, height})
{
}

void Record::updateInput(const double& radius, const double& height)
{
	this->volume = input.first * input.first * pi * input.second;
}

void Record::updateInput(Record record)
{
	this->input = record.getInput();
	this->volume = record.getVolume();
}

std::pair<double, double> Record::getInput()
{
	return this->input;
}

double Record::getVolume() const
{
	return this->volume;
}
