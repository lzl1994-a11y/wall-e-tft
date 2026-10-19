#include "domain/HostNetworkScreen.h"
#include <cassert>
#include <iostream>
#include <string>

int main() {
  auto parse = [](const std::string& value) {
    WallE::HostNetworkScreen screen;
    return WallE::parseHostNetworkScreen(reinterpret_cast<const uint8_t*>(value.data()), value.size(), screen);
  };
  assert(parse("hostnet:4294967295|1|setup|10.42.0.1|8080|WALL-E-1234|12345678"));
  assert(parse("hostnet:1|1|connected|192.168.0.5|8080||"));
  assert(parse("hostnet:2|1|done|192.168.0.5|8080||"));
  assert(parse("hostnet:3|1|syncing|192.168.0.5|8080||"));
  assert(parse("hostnet:4|1|partial|192.168.0.5|8080||"));
  assert(!parse("hostnet:4294967296|1|setup|10.42.0.1|8080|a|b"));
  assert(!parse("hostnet:1|2|setup|10.42.0.1|8080|a|b"));
  assert(!parse("hostnet:1|1|other|10.42.0.1|8080|a|b"));
  assert(!parse("hostnet:1|1|setup|256.1.1.1|8080|a|b"));
  assert(!parse("hostnet:1|1|setup|10.42.0.1|0|a|b"));
  assert(!parse("hostnet:1|1|setup|10.42.0.1|8080|a|b|c"));
  assert(!parse("hostnet:1|1|setup|10.42.0.1|8080|a|b\n"));
  assert(!parse(std::string(200, 'a')));
  std::cout << "Host network screen protocol tests passed\n";
}
