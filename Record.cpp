#include "Record.h"

Record::Record(std::pair<double, double> input)
{
	this->input = input;
	this->volume = input.first * input.first * this->PI * input.second;
}

Record::Record(const double& radius, const double& height) : Record(std::pair<double, double>{radius, height})
{

}

void Record::updateInput(const double& radius, const double& height)
{
	this->input = input;
	this->volume = input.first * input.first * this->PI * input.second;
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

double Record::getVolume()
{
	return this->volume;
}