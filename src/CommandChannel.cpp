//
// Created by stephane bourque on 2021-04-10.
//

#include "CommandChannel.h"
#include "uCentral.h"
#include "uCentralWebSocketServer.h"
#include "uFileUploader.h"
#include "uStorageService.h"
#include "uCentralRESTAPIServer.h"
#include "uCommandManager.h"
#include "uAuthService.h"
#include <boost/algorithm/string.hpp>

namespace uCentral::CommandChannel {

	Service * Service::instance_ = nullptr;

	int Start() {
		return uCentral::CommandChannel::Service::instance()->Start();
	}

	void Stop() {
		uCentral::CommandChannel::Service::instance()->Stop();
	}

	std::string  ProcessCommand(const std::string &Command) {
		std::vector<std::string>	Tokens{};
		std::string Result{"OK"};
		Poco::Logger	& Logger = uCentral::Daemon::instance().logger();

		size_t pos = 0, old_pos = 0 ;

		try {
			Logger.notice(Poco::format("COMMAND: %s",Command));

			while((pos = Command.find(' ', old_pos)) != std::string::npos) {
				Tokens.push_back(Command.substr(old_pos,pos-old_pos));
				old_pos = pos + 1 ;
			}

			Tokens.push_back(Command.substr(old_pos));
			boost::algorithm::to_lower(Tokens[0]);
			boost::algorithm::to_lower(Tokens[1]);

			if(Tokens[0]=="set") {
				if(Tokens[1]=="loglevel") {
					if(!uCentral::Daemon::SetSubsystemLogLevel(Tokens[3],Tokens[2]))
						Result =  "ERROR: Invalid: set logLevel subsystem name:" + Tokens[3];
				}
			} else if(Tokens[0]=="get") {
				if(Tokens[1]=="loglevel") {
					std::cout << "LogLevels:" << std::endl;
					std::cout << " Auth: " << uCentral::Auth::Service().Logger().getLevel() << std::endl;
					std::cout << " uFileUploader: " << uCentral::uFileUploader::Service().Logger().getLevel() << std::endl;
					std::cout << " WebSocket: " << uCentral::WebSocket::Service().Logger().getLevel() << std::endl;
					std::cout << " Storage: " << uCentral::Storage::Service().Logger().getLevel() << std::endl;
					std::cout << " RESTAPI: " << uCentral::RESTAPI::Service().Logger().getLevel() << std::endl;
					std::cout << " CommandManager: " << uCentral::CommandManager::Service().Logger().getLevel() << std::endl;
					std::cout << " DeviceRegistry: " << uCentral::DeviceRegistry::Service().Logger().getLevel() << std::endl;
				} else if (Tokens[1]=="stats") {

				} else {
					Result =  "ERROR: Invalid: get command:" + Tokens[1];
				}
			} else if(Tokens[0]=="restart") {

			} else if(Tokens[0]=="stop") {

			} else if(Tokens[0]=="stats") {

			} else {
				Result = "ERROR: Invalid command: " + Tokens[0];
			}
			Logger.notice(Poco::format("COMMAND-RESULT: %s",Result));
		}
		catch ( const Poco::Exception & E) {
			Logger.warning(Poco::format("COMMAND: Poco exception %s in performing command.",E.displayText()));
		}
		catch ( const std::exception & E) {
			Logger.warning(Poco::format("COMMAND: std::exception %s in performing command.",std::string(E.what())));
		}

		return Result;
	}

	/// This class handles all client connections.
	class UnixSocketServerConnection: public Poco::Net::TCPServerConnection
	{
	  public:
		explicit UnixSocketServerConnection(const Poco::Net::StreamSocket & S, Poco::Logger & Logger):
			TCPServerConnection(S),
		 	Logger_(Logger)
		{
		}

		void run() override
		{
			try
			{
				std::string Message;
				std::vector<char>	buffer(1024);
				int n = 1;
				while (n > 0)
				{
					n = socket().receiveBytes(&buffer[0], (int)buffer.size());
					buffer[n] = '\0';
					Message += &buffer[0];
					if(buffer.size() > n && !Message.empty())
					{
						ProcessCommand(Message);
						Message.clear();
					}
				}
			}
			catch (Poco::Exception& exc)
			{
				std::cerr << "Error: " << exc.displayText() << std::endl;
			}
		}

	  private:
		Poco::Logger	& Logger_;
		static inline void EchoBack(const std::string & message)
		{
			std::cout << "Message: " << message << std::endl;
			// socket().sendBytes(message.c_str(), (int)message.size());
		}
	};

	class UnixSocketServerConnectionFactory: public Poco::Net::TCPServerConnectionFactory
	{
	  public:
		explicit UnixSocketServerConnectionFactory() :
			Logger_(Service::instance()->Logger())
		{
		}

		Poco::Net::TCPServerConnection* createConnection(const Poco::Net::StreamSocket& socket) override
		{
			return new UnixSocketServerConnection(socket,Logger_);
		}
	  private:
		Poco::Logger & Logger_;
	};

	Service::Service() noexcept:
		SubSystemServer("Authentication", "AUTH-SVR", "authentication")
	{
	}

	void Service::Stop() {
		Logger_.notice("Stopping...");
		Srv_->stop();
	}

	int Service::Start() {
		Poco::File	F(uCentral::ServiceConfig::getString("ucentral.system.commandchannel","/tmp/app.ucentralgw"));
		try { F.remove(); } catch (...) {};
		SocketFile_ = std::make_unique<Poco::File>(F);
		UnixSocket_ = std::make_unique<Poco::Net::SocketAddress>(Poco::Net::SocketAddress::UNIX_LOCAL, SocketFile_->path());
		Svs_ = std::make_unique<Poco::Net::ServerSocket>(*UnixSocket_);
		Srv_ = std::make_unique<Poco::Net::TCPServer>(new UnixSocketServerConnectionFactory, *Svs_);
		Srv_->start();
		Logger_.notice("Starting...");
		return 0;
	}
};