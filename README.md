# ISAM Database
This project is a simulation of a simple DBMS (_Database Management System_) that uses [indexed sequential access method](https://en.wikipedia.org/wiki/ISAM).

## Usage
The DBMS works on records that contain information about trapeziod volumes. They must hold the data on the `radius` and `height`. 
<hr>
However, the goal was simulate a properly implemented ISAM structure instead of focusing on records. Therefore records have data randomly generated, but the user has to **specify record ID**. Thanks to this, the system manages the records in a way that has minimal performance costs.

## Features
- Record operations
  - Insert
  - Update
  - Read
  - Delete
- Reorganizing on command
- Automatic backups
- Printing
  - amount of disk operations
  - data of the areas

## About technology
The code is purely written in C++ and is compatible with version 11 and higher.
