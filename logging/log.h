#ifndef LOG_H
#define LOG_H

#include <iostream>
#include <string>
#include <chrono>
#include <ctime>
#include <time.h>                                                               
#include <map>

using namespace std;

/**
 * Log class
 *
 * Usage:
 *
 * #include "Log.h"
 *
 * int main(int argc, char* argv[]){
 *   // create a new logger instance by calling the constructor:
 *   Log* log = new Log(debugLevel); // debugLevel's type is integer.
 * 0:DEBUG(ALL), 10:INFO, 20:WARN, 30:ERROR, 99:NONE
 *   // or by passing a debug option as app's arguments:
 * --debug=(debug|info|warn|error)
 *   // Log* log = Log::getInstanceFromArgs(argc, argv);
 *
 *   log->debug("hoge", "foo", 123);
 *   log->info("hoge", "foo", 123);
 *   log->warn("hoge", "foo", 123);
 *   log->error("hoge", "foo", 123);
 */
class Log {
 public:
  /* loglevel for console debug logs */
  int debugLevel = 99;  // 99:NONE (nothing logs)

  /** constructor */
  Log(int debugLevel) { this->debugLevel = debugLevel; }

  /** creates and returns a Log instance from arguments */
  static Log* getInstanceFromArgs(int argc, char* argv[]) {
    int logLevel = 99;  // 99:NONE
    //
    // parsing application's arguments:
    //
    map<string, string> m;
    // Search each command-line argument.
    for (int count = 0; count < argc; count++) {
      string line = string(argv[count]);
      if (line.find_first_of("-") != 0) continue;
      string key = "";
      string val = "";
      int len = line.length();
      bool keyStarted = false;
      bool valStarted = false;
      for (int i = 0; i < len; i++) {
        char c = line[i];
        if (!keyStarted && c == '-') continue;
        keyStarted = true;  // key part started.
        if (keyStarted && c == '=') {
          valStarted = true;  // value part started.
          continue;
        }
        if (keyStarted && !valStarted) {
          key.push_back(c);
        } else {
          val.push_back(c);
        }
      }
      m[key] = val;
    }

    string debugLevel;
    if (m.find("debug") != m.end()) {
      debugLevel = m["debug"];
      if (debugLevel == "") debugLevel = "all";
    }
    if (debugLevel == "all" || debugLevel == "debug")
      logLevel = 0;
    else if (debugLevel == "info")
      logLevel = 10;
    else if (debugLevel == "warn")
      logLevel = 20;
    else if (debugLevel == "error")
      logLevel = 30;
    return new Log(logLevel);
  }

  /** parse arguments */
  map<string, string> parseArgs(int argc, char* argv[]) {
    map<string, string> m;
    // Search each command-line argument.
    for (int count = 0; count < argc; count++) {
      string line = string(argv[count]);
      if (line.find_first_of("-") != 0) continue;
      string key = "";
      string val = "";
      int len = line.length();
      bool keyStarted = false;
      bool valStarted = false;
      for (int i = 0; i < len; i++) {
        char c = line[i];
        if (!keyStarted && c == '-') continue;
        keyStarted = true;  // key part started.
        if (keyStarted && c == '=') {
          valStarted = true;  // value part started.
          continue;
        }
        if (keyStarted && !valStarted) {
          key.push_back(c);
        } else {
          val.push_back(c);
        }
      }
      m[key] = val;
    }
    return m;
  }

  /** Get a current ISO date string. */
  string getISODate() {
    std::timespec ts;
    if (std::timespec_get(&ts, TIME_UTC) == 0) {
      return "fail to get timestamp";
    }
    string s = std::ctime(&ts.tv_sec);
    return s;
  }

  template <typename T>
  void log_argument(T t) {
    cout << t << " ";
  }
  bool shouldLog(int debugLevel) {
    if (this->debugLevel >= 99) return false;  // NONE
    if (this->debugLevel <= debugLevel) return true;
    return false;
  }
  /** debug log */
  template <typename... Args>
  void debug(Args&&... args) {
    if (!this->shouldLog(0)) return;  // check global.debug flag.
    cout << this->getISODate() << " DEBUG: ";
    cout << endl;
  }
  /** info log */
  template <typename... Args>
  void info(Args&&... args) {
    if (!this->shouldLog(10)) return;  // check global.debug flag.
    cout << this->getISODate() << " INFO : ";
    cout << endl;
  }
  /** warn log */
  template <typename... Args>
  void warn(Args&&... args) {
    if (!this->shouldLog(20)) return;  // check global.debug flag.
    cout << this->getISODate() << " WARN : ";
    cout << endl;
  }
  /** error log */
  template <typename... Args>
  void error(Args&&... args) {
    if (!this->shouldLog(30)) return;  // check global.debug flag.
    cout << this->getISODate() << " ERROR: ";
    cout << endl;
  }
};

#endif  // LOG_H