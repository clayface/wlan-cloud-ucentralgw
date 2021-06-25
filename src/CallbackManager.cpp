//
//	License type: BSD 3-Clause License
//	License copy: https://github.com/Telecominfraproject/wlan-cloud-ucentralgw/blob/master/LICENSE
//
//	Created by Stephane Bourque on 2021-03-04.
//	Arilia Wireless Inc.
//

#include "CallbackManager.h"
#include "Daemon.h"

#include "Poco/Net/HTTPClientSession.h"
#include "Poco/Net/HTTPSClientSession.h"
#include "Poco/Net/HTTPRequest.h"
#include "Poco/Net/HTTPResponse.h"
#include "Poco/URI.h"

namespace uCentral {
	class CallbackManager *CallbackManager::instance_ = nullptr;

	CallbackManager::CallbackManager() noexcept:
	  SubSystemServer("CallbackManager", "CBACK-MGR", "ucentral.callback")
	{
	}

	int CallbackManager::Start() {
		Logger_.notice("Starting...");
		Mgr_.start(*this);
		return 0;
	}

	bool CallbackManager::InitHosts() {
		// get all the hosts we are registering with and register ourselves...

		if(Daemon()->ConfigGetString("ucentral.callback.enable","false") == "false") {
			Logger_.information("CALLBACK system disabled.");
			return false;
		}

		MyIDCallbackId_ = Daemon()->ConfigGetString("ucentral.callback.id","");
		if(MyIDCallbackId_.empty()) {
			Logger_.information("CALLBACK system disabled. No CallbackID present in ucentral.callback.id");
			return false;
		}

		//	now get all the hosts we need to register with...
		auto Index = 0 ;
		while(true) {
			std::string root = "ucentral.callback." + std::to_string(Index);

			auto Local = Daemon()->ConfigGetString(root + ".local","");
			auto Remote = Daemon()->ConfigGetString(root + ".remote","");
			auto LocalKey = Daemon()->ConfigGetString(root + ".localkey","");
			auto RemoteKey = Daemon()->ConfigGetString(root + ".localkey","");
			auto Topics = Daemon()->ConfigGetString(root + ".topics","");

			if(Local.empty() || Remote.empty() || LocalKey.empty() || Topics.empty() || RemoteKey.empty())
				break;

			CallbackHost H{
				.Local = "https://" + Local + "/api/v1/callbackChannel",
				.LocalKey = LocalKey,
				.Remote = "https://" + Remote + "/api/v1/callbackChannel",
				.RemoteKey = RemoteKey,
				.Topics = Topics,
				.LastContact = 0,
				.NextContact = 0,
				.Registered = false
			};
			Hosts_.push_back(H);
			Index++;
		}

		return true;
	}

	bool DoRequest(Poco::Net::HTTPSClientSession& Session, Poco::Net::HTTPRequest& Request, Poco::Net::HTTPResponse& Response)
	{
		std::string Content{R"lit({ "comment" : "registration from uCentralGW" })lit"};
		std::stringstream Body(Content);
		Request.setContentType("application/json");
		Request.setContentLength(Content.length());
		std::ostream& OS = Session.sendRequest(Request);
		Poco::StreamCopier::copyStream(Body, OS);
		Session.receiveResponse(Response);
		return (Response.getStatus() == Poco::Net::HTTPResponse::HTTP_OK);
	}

	bool CallbackManager::RegisterHosts() {

		if(MyIDCallbackId_.empty())
			return false;

		for(auto &i:Hosts_) {
			if(!i.Registered || (time(nullptr)-i.LastContact)>300) {
				Poco::URI Uri(i.Remote);

				Uri.addQueryParameter("subscribe", "true");
				Uri.addQueryParameter("uri", i.Local);
				Uri.addQueryParameter("topics", i.Topics);
				Uri.addQueryParameter("key", i.LocalKey);
				Uri.addQueryParameter("id", MyIDCallbackId_);

				Poco::Net::HTTPSClientSession Session(Uri.getHost(), Uri.getPort());
				Poco::Net::HTTPRequest Request(Poco::Net::HTTPRequest::HTTP_POST,
											   Uri.getPathAndQuery(),
											   Poco::Net::HTTPMessage::HTTP_1_1);
				Request.add("X-API-KEY", i.RemoteKey);

				Poco::Net::HTTPResponse Response;

				i.LastContact = time(nullptr);
				i.Registered = DoRequest(Session, Request, Response);
			}
		}

		return true;
	}

	void CallbackManager::run() {
		Running_ = true;

		uint64_t LastContact = time(nullptr);

		InitHosts();
		RegisterHosts();

		while(Running_) {
			if((time(nullptr) - LastContact) >300) {
				RegisterHosts();
				LastContact = time(nullptr);
			}
			if(Calls_.empty()) {
				Poco::Thread::sleep(2000);
			} else {

				CallBackMessage E;
				{
					SubMutexGuard Guard(Mutex_);
					E = Calls_.front();
				}

				std::cout << "Call: " << E.Message << " JSON:" << E.JSONDoc << std::endl;

				{
					SubMutexGuard Guard(Mutex_);
					Calls_.pop();
				}
			}
		}
	}

	void CallbackManager::Stop() {
		SubMutexGuard Guard(Mutex_);

		Logger_.notice("Stopping...");
		Running_ = false ;
		Mgr_.join();
	}

	bool CallbackManager::AddMessage(const CallBackMessage &Msg) {
		SubMutexGuard Guard(Mutex_);

		Calls_.push(Msg);

		return true;
	}

}  // end of namespace
