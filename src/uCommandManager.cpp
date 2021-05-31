//
//	License type: BSD 3-Clause License
//	License copy: https://github.com/Telecominfraproject/wlan-cloud-ucentralgw/blob/master/LICENSE
//
//	Created by Stephane Bourque on 2021-03-04.
//	Arilia Wireless Inc.
//

#include <algorithm>
#include "uCommandManager.h"

#include "RESTAPI_objects.h"
#include "RESTAPI_handler.h"
#include "uStorageService.h"
#include "uDeviceRegistry.h"
#include "uCentralProtocol.h"
#include "uCommandManager.h"

#define DBG		std::cout << __LINE__ << "   " __FILE__ << std::endl;

namespace uCentral::CommandManager {

    Service *Service::instance_ = nullptr;

    Service::Service() noexcept: uSubSystemServer("CmdManager", "CMD_MGR", "command.manager"),
            Manager_(Logger_)
    {
    }

    int Start()
    {
        return Service::instance()->Start();
    }

    void Stop()
    {
        Service::instance()->Stop();
    }

	void Janitor() {
		Service::instance()->Janitor();
	}

    void WakeUp()
    {
        uCentral::CommandManager::Service::instance()->WakeUp();
    }

	void PostCommandResult(const std::string &SerialNumber, Poco::JSON::Object::Ptr Result) {
		Service::instance()->PostCommandResult(SerialNumber, Result);
	}

	bool SendCommand( const std::string & SerialNumber, const std::string & Method, const Poco::JSON::Object &Params, std::promise<Poco::JSON::Object::Ptr> Promise) {
		return Service::instance()->SendCommand(SerialNumber, Method, Params, std::move(Promise));
	}

	bool SendCommand( const std::string & SerialNumber, const std::string & Method, const Poco::JSON::Object &Params, const std::string & UUID) {
		return Service::instance()->SendCommand(SerialNumber, Method, Params, UUID);
	}

	void Manager::run() {
        while(!Stop_)
        {
            Poco::Thread::trySleep(10000);
            std::vector<uCentral::Objects::CommandDetails> Commands;

            if(uCentral::Storage::GetReadyToExecuteCommands(0,1000,Commands))
            {
                for(auto & Cmd: Commands)
                {
                    if(!uCentral::DeviceRegistry::SendCommand(Cmd)) {
                        Logger_.information(Poco::format("Failed to send command '%s' to %s",Cmd.Command,Cmd.SerialNumber));
                    }
                }
            }
			Janitor();
        }
    }

    int Service::Start() {
        Logger_.notice("Starting...");
        ManagerThread.start(Manager_);
        return 0;
    }

    void Service::Stop() {
        Logger_.notice("Stopping...");
        Manager_.stop();
        ManagerThread.join();
    }

    void Service::WakeUp() {
        Logger_.notice("Waking up..");
        ManagerThread.wakeUp();
    }

	void Service::Janitor() {
		SubMutexGuard G(SubMutex);
		uint64_t Now = time(nullptr);
		for(auto i = Age_.begin(); i!= Age_.end();)
			if((Now-i->first)>300)
				Age_.erase(i++);
			else
				++i;
	}

	bool Service::SendCommand(const std::string &SerialNumber,
							  const std::string &Method,
							  const Poco::JSON::Object &Params,
							  std::promise<Poco::JSON::Object::Ptr> promise) {
		SubMutexGuard G(SubMutex);

		Poco::JSON::Object	CompleteRPC;
		CompleteRPC.set(uCentralProtocol::JSONRPC,uCentralProtocol::JSONRPC_VERSION);
		CompleteRPC.set(uCentralProtocol::ID,Id_);
		CompleteRPC.set(uCentralProtocol::METHOD, Method );
		CompleteRPC.set(uCentralProtocol::PARAMS, Params);
		std::stringstream ToSend;
		Poco::JSON::Stringifier::stringify(CompleteRPC,ToSend);

		OutStandingRequests_[Id_] = std::move(promise);
		Age_[Id_] = time(nullptr);
		Id_++;
		return uCentral::DeviceRegistry::SendFrame(SerialNumber, ToSend.str());
	}

	bool Service::SendCommand( 	const std::string & SerialNumber,
						 const std::string & Method,
						 const Poco::JSON::Object &Params,
						 const std::string & UUID) {
		SubMutexGuard G(SubMutex);

		Poco::JSON::Object	CompleteRPC;
		CompleteRPC.set(uCentralProtocol::JSONRPC,uCentralProtocol::JSONRPC_VERSION);
		CompleteRPC.set(uCentralProtocol::ID,Id_);
		CompleteRPC.set(uCentralProtocol::METHOD, Method );
		CompleteRPC.set(uCentralProtocol::PARAMS, Params);
		std::stringstream ToSend;
		Poco::JSON::Stringifier::stringify(CompleteRPC,ToSend);

		OutStandingCommands_[Id_] = UUID;
		Age_[Id_] = time(nullptr);
		Id_++;
		return uCentral::DeviceRegistry::SendFrame(SerialNumber, ToSend.str());

	}


void Service::PostCommandResult(const std::string &SerialNumber, Poco::JSON::Object::Ptr Obj) {
		if(!Obj->has(uCentralProtocol::ID)){
			Logger_.error("Invalid RPC response.");
			return;
		}

		SubMutexGuard G(SubMutex);

		uint64_t ID = Obj->get(uCentralProtocol::ID);
		auto RPC = OutStandingRequests_.find(ID);
		Age_.erase(ID);
		if(RPC != OutStandingRequests_.end()) {
			RPC->second.set_value(std::move(Obj));
			OutStandingRequests_.erase(RPC);
		} else {
			auto Cmd = OutStandingCommands_.find(ID);
			if(Cmd!=OutStandingCommands_.end()) {
				uCentral::Storage::CommandCompleted(Cmd->second, Obj, true);
				OutStandingCommands_.erase(Cmd);
			}
		}
	}


}  // namespace