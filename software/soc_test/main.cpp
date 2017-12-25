#include "nios.h"
#include <iostream>
#include <limits.h>
#include <string.h>
#include <vector>
#include <algorithm>
#include <thread>

using namespace std;

shared_ptr<Nios> nios;

struct ConsoleCmd {
  string cmd;
  vector<int32_t> params;
};

vector<string> split(const string& s, const string& delim, const bool keep_empty = true)
{
  vector<string> result;
  if(delim.empty())
  {
    result.push_back(s);
    return result;
  }
  string::const_iterator substart = s.begin(), subend;
  while(1)
  {
    subend = search(substart, s.end(), delim.begin(), delim.end());
    string temp(substart, subend);
    if(keep_empty || !temp.empty())
    {
      result.push_back(temp);
    }
    if (subend == s.end())
    {
      break;
    }
    substart = subend + delim.size();
  }
  return result;
}

ConsoleCmd parser(std::string str)
{
  ConsoleCmd c;
  vector<string> sstr;
  int32_t val;

  c.cmd.clear();
  c.params.clear();
  if((str[0] == '\0') || (str[0] == '\n'))
  {
    return c;
  }
  sstr = split(str, " ", false);
  c.cmd = sstr[0];
  if(sstr.size() > 1)
  {
    for(uint i = 1; i < sstr.size(); i++)
    {
      val = stoi(sstr[i], NULL, 10);
      c.params.push_back(val);
    }
  }
  return c;
}

int main(int argc, char* argv[])
{
  ConsoleCmd c;
  char rawstr[UCHAR_MAX];
  string str;
  nios.reset(new Nios());

  cout << "Enter command (\"read\"(\"r\"), \"write\"(\"w\"), \"reverse\"), \"q\" to exit" << endl;
  while(1)
  {
    memset(rawstr, 0, strlen(rawstr));
    printf("> ");
    fgets(rawstr, UCHAR_MAX, stdin);
    if((rawstr[0] == '\0') || (rawstr[0] == '\n'))
    {
      continue;
    }
    str = rawstr;
    str.pop_back();
    try {
      c = parser(str);
    }
    catch (const std::invalid_argument& e) {
      cerr << "Input error " << e.what() << endl;
      continue;
    }
    if(c.cmd.empty())
    {
      continue;
    }
    else if((c.cmd == "quit") || (c.cmd == "q"))
    {
      cout << "Bye!" << endl;
      break;
    }
    else if((c.cmd == "read") || (c.cmd == "r"))
    {
      nios->ledRead();
    }
    else if((c.cmd == "write") || (c.cmd == "w"))
    {
      if(c.params.size() >= 1)
      {
	nios->ledWrite(c.params[0]);
      }
      else
      {
        cout << "Please choose led" << endl;
      }
    }
    else if(c.cmd == "reverse")
    {
      nios->ledReverse();
    }
  }
  return 0;
}
