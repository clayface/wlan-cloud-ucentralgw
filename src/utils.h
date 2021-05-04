//
// Created by stephane bourque on 2021-05-03.
//

#ifndef UCENTRALGW_UTILS_H
#define UCENTRALGW_UTILS_H

#include <vector>
#include <string>

namespace uCentral::Utils {

	[[nodiscard]] std::vector<std::string> Split(const std::string &List, char Delimiter=',');

}
#endif // UCENTRALGW_UTILS_H
