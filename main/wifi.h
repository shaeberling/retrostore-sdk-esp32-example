#pragma once

#ifndef _WINSTON_WIFI_H_
#define _WINSTON_WIFI_H_

#include <string>

enum {
    WIFI_CONNECTED,
};


class Wifi {
 public:
  void connect(const std::string& ssid, const std::string& password);
};

#endif /* _WINSTON_WIFI_H_ */