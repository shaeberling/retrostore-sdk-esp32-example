// Program to test the RetroStore SDK and service.

#include <iostream>

#include "retrostore.h"

using namespace std;
using namespace retrostore;

int main () {
  RetroStore rs;
  rs.PrintVersion();
  cout << "Hello World!\n";
  return 0;
}