//
// Created by stephane bourque on 2021-10-14.
//

#ifndef UCENTRALGW_RESTAPI_BLACKLIST_LIST_H
#define UCENTRALGW_RESTAPI_BLACKLIST_LIST_H

#include "RESTAPI_handler.h"

namespace OpenWifi {
	class RESTAPI_blacklist_list : public RESTAPIHandler {
	  public:
		RESTAPI_blacklist_list(const RESTAPIHandler::BindingMap &bindings, Poco::Logger &L, RESTAPI_GenericServer & Server, bool Internal)
		: RESTAPIHandler(bindings, L,
						 std::vector<std::string>{Poco::Net::HTTPRequest::HTTP_GET,
												  Poco::Net::HTTPRequest::HTTP_OPTIONS},
												  Server,
												  Internal) {}
												  static const std::list<const char *> PathName() { return std::list<const char *>{"/api/v1/blacklist"};}
		void DoGet() final;
		void DoDelete() final {};
		void DoPost() final {};
		void DoPut() final {};
	};
}

#endif // UCENTRALGW_RESTAPI_BLACKLIST_LIST_H