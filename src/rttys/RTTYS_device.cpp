//
// Created by stephane bourque on 2021-11-23.
//

#include "RTTYS_device.h"
#include "rttys/RTTYS_server.h"

namespace OpenWifi {

	RTTY_Device_ConnectionHandler::RTTY_Device_ConnectionHandler(Poco::Net::StreamSocket& socket,
															 Poco::Net::SocketReactor & reactor):
		socket_(socket),
		reactor_(reactor),
		Logger_(RTTYS_server()->Logger())
	{
		reactor_.addEventHandler(socket_, Poco::NObserver<RTTY_Device_ConnectionHandler, Poco::Net::ReadableNotification>(*this, &RTTY_Device_ConnectionHandler::onSocketReadable));
		reactor_.addEventHandler(socket_, Poco::NObserver<RTTY_Device_ConnectionHandler, Poco::Net::ShutdownNotification>(*this, &RTTY_Device_ConnectionHandler::onSocketShutdown));
		// reactor_.addEventHandler(socket_, Poco::NObserver<RTTY_Device_ConnectionHandler, Poco::Net::WritableNotification>(*this, &RTTY_Device_ConnectionHandler::onSocketWritable));
	}

	RTTY_Device_ConnectionHandler::~RTTY_Device_ConnectionHandler()
	{
		reactor_.removeEventHandler(socket_, Poco::NObserver<RTTY_Device_ConnectionHandler, Poco::Net::ReadableNotification>(*this, &RTTY_Device_ConnectionHandler::onSocketReadable));
		// reactor_.removeEventHandler(socket_, Poco::NObserver<RTTY_Device_ConnectionHandler, Poco::Net::WritableNotification>(*this, &RTTY_Device_ConnectionHandler::onSocketWritable));
		reactor_.removeEventHandler(socket_, Poco::NObserver<RTTY_Device_ConnectionHandler, Poco::Net::ShutdownNotification>(*this, &RTTY_Device_ConnectionHandler::onSocketShutdown));
		socket_.close();
		if(!id_.empty()) {
			RTTYS_server()->DeRegister(id_, this);
			RTTYS_server()->Close(id_);
		} else {
		}
	}

	std::string RTTY_Device_ConnectionHandler::SafeCopy( const u_char * buf, int MaxSize, int & NewPos) {
		std::string     S;
		while(NewPos<MaxSize && buf[NewPos]!=0) {
			S += buf[NewPos++];
		}

		if(buf[NewPos]==0)
			NewPos++;

		return S;
	}

	void RTTY_Device_ConnectionHandler::PrintBuf(const u_char * buf, int size) {

		std::cout << "======================================" << std::endl;
		while(size) {
			std::cout << std::hex << (int) *buf++ << " ";
			size--;
		}
		std::cout << std::endl;
		std::cout << "======================================" << std::endl;
	}

	int RTTY_Device_ConnectionHandler::SendMessage(RTTY_MSG_TYPE Type, const u_char * Buf, int BufLen) {
		u_char outBuf[ 256 ]{0};
		auto msg_len = BufLen + 1 ;
		auto total_len = msg_len + 3 ;
		outBuf[0] = Type;
		outBuf[1] = (msg_len >> 8);
		outBuf[2] = (msg_len & 0x00ff);
		outBuf[3] = sid_;
		std::memcpy(&outBuf[4], Buf, BufLen);
		// PrintBuf(outBuf,total_len);
		return socket_.sendBytes(outBuf,total_len) == total_len;
	}

	int RTTY_Device_ConnectionHandler::SendMessage(RTTY_MSG_TYPE Type, std::string &S ) {
		u_char outBuf[ 256 ]{0};
		int msg_len = S.size()+1 ;
		int total_len= msg_len+3;
		outBuf[0] = Type;
		outBuf[1] = (msg_len >> 8);
		outBuf[2] = (msg_len & 0x00ff);
		outBuf[3] = sid_;
		std::strcpy((char*)&outBuf[4],S.c_str());
		// PrintBuf(outBuf,total_len);
		return socket_.sendBytes(outBuf,total_len) == total_len;
	}

	int RTTY_Device_ConnectionHandler::SendMessage(RTTY_MSG_TYPE Type) {
		u_char outBuf[ 256 ]{0};
		auto msg_len = 0 ;
		auto total_len= msg_len+3+1;
		outBuf[0] = Type;
		outBuf[1] = (msg_len >> 8);
		outBuf[2] = (msg_len & 0x00ff);
		outBuf[3] = sid_;
		return socket_.sendBytes(outBuf,total_len) == total_len;
	}

	void RTTY_Device_ConnectionHandler::SendToClient(const u_char *Buf, int len) {
		auto Client = RTTYS_server()->GetClient(id_);
		if(Client!= nullptr)
			Client->SendData(Buf,len);
	}

	void RTTY_Device_ConnectionHandler::SendToClient(const std::string &S) {
		auto Client = RTTYS_server()->GetClient(id_);
		if(Client!= nullptr)
			Client->SendData(S);
	}

	void RTTY_Device_ConnectionHandler::KeyStrokes(const u_char *buf, size_t len) {
		u_char outBuf[16]{0};

		if(len>(sizeof(outBuf)-5))
			return;

		auto total_len = 3 + 1 + len-1;
		outBuf[0] = msgTypeTermData;
		outBuf[1] = 0 ;
		outBuf[2] = len +1-1;
		outBuf[3] = sid_;
		memcpy( &outBuf[4], &buf[1], len-1);
		socket_.sendBytes(outBuf, total_len);
		// PrintBuf(outBuf, total_len);
	}

	void RTTY_Device_ConnectionHandler::WindowSize(int cols, int rows) {
		u_char	outBuf[32]{0};
		outBuf[0] = msgTypeWinsize;
		outBuf[1] = 0 ;
		outBuf[2] = 4 + 1 ;
		outBuf[3] = sid_;
		outBuf[4] = cols >> 8 ;
		outBuf[5] = cols & 0x00ff;
		outBuf[6] = rows >> 8;
		outBuf[7] = rows & 0x00ff;
		// PrintBuf(outBuf,8);
		socket_.sendBytes(outBuf,8);
	}

	bool RTTY_Device_ConnectionHandler::Login() {
		u_char outBuf[8]{0};
		outBuf[0] = msgTypeLogin;
		outBuf[1] = 0;
		outBuf[2] = 0;
		// PrintBuf(outBuf,3);
		socket_.sendBytes(outBuf,3 );
		return true;
	}

	bool RTTY_Device_ConnectionHandler::Logout() {
		u_char outBuf[64];
		outBuf[0] = msgTypeLogout;
		outBuf[1] = 0;
		outBuf[2] = 1;
		outBuf[3] = sid_;
		Logger().debug(fmt::format("Device {} logging out", id_));
		// PrintBuf(outBuf,4);
		socket_.sendBytes(outBuf,4 );
		return true;
	}

	std::string RTTY_Device_ConnectionHandler::ReadString() {
		std::string Res;

		while(inBuf_.used()) {
			char C;
			inBuf_.read(&C,1);
			if(C==0) {
				break;
			}
			Res += C;
		}

		return Res;
	}

	void RTTY_Device_ConnectionHandler::onSocketReadable([[maybe_unused]] const Poco::AutoPtr<Poco::Net::ReadableNotification>& pNf)
	{
		try
		{
			// memset(&inBuf[0],0,sizeof inBuf);
			std::size_t needed = socket_.available();
			if((inBuf_.size()-inBuf_.used())<needed) {
				std::cout << "Not enough room..." << std::endl;
				return;
			}

			auto received = socket_.receiveBytes(inBuf_);

			if(inBuf_.used()==0 || received<0) {
				std::cout << "." ;
				return;
			}

			int loops = 1;
			bool done=false;
			while(!done && inBuf_.used()>0) {
				std::cout << "Loop:" << loops++ << "  --> " << waiting_for_bytes_ << "   " << (int) last_command_ << std::endl;
				size_t MsgLen;
				if(waiting_for_bytes_==0) {
					u_char msg[3];
					if (inBuf_.read((char *)&msg[0], 3) != 3)
						break;
					MsgLen = (size_t)msg[1] * 256 + (size_t)msg[2];
					std::cout << "AV:" << inBuf_.used() << " LEN:" << MsgLen << " B1:" << (uint32_t) msg[1] << "  B2:" << (uint32_t)msg[2] << std::endl;

					if (msg[0] > msgTypeMax) {
						std::cout << "Bad message type:" << (int)msg[0] << std::endl;
						Logger().debug(fmt::format("Bad message for Session: {}", id_));
						return delete this;
					}

					if (MsgLen > inBuf_.used()) {
						std::cout << "Not enough data for the message length:" << MsgLen
								  << std::endl;
						waiting_for_bytes_ = MsgLen ;
					} else {
						waiting_for_bytes_ = 0;
					}

					std::cout << "Command: " << (int)msg[0] << std::endl;
					last_command_ = msg[0];
				} else {
					last_command_ = msgTypeTermData;
				}


				switch (last_command_) {
				case msgTypeRegister: {
					id_ = ReadString();
					desc_ = ReadString();
					token_ = ReadString();
					std::cout << "ID:" << id_ << " DESC:" << desc_ << " TOK:" << token_ << std::endl;
					if (RTTYS_server()->ValidEndPoint(id_, token_)) {
						if (!RTTYS_server()->AmIRegistered(id_, token_, this)) {
							u_char OutBuf[12];
							OutBuf[0] = msgTypeRegister;
							OutBuf[1] = 0;
							OutBuf[2] = 4;
							OutBuf[3] = 0;
							OutBuf[4] = 'O';
							OutBuf[5] = 'K';
							OutBuf[6] = 0;
							socket_.sendBytes(OutBuf, 7);
							RTTYS_server()->Register(id_, this);
							serial_ = RTTYS_server()->SerialNumber(id_);
							Logger().debug(
								fmt::format("Registration for SerialNumber: {}, Description: {}",
											serial_, desc_));
						} else {
							Logger().debug(fmt::format(
								"Registration for SerialNumber: {}, already done", serial_));
						}
					} else {
						Logger().debug(fmt::format("Registration failed - invalid (id,token) pair. for Session: {}, Description: {}",
												   id_, desc_));
						return delete this;
					}
				} break;

				case msgTypeLogin: {
					Logger().debug(fmt::format(
						"Device created session for SerialNumber: {}, session: {}", serial_, id_));
					nlohmann::json doc;
					char Error;
					inBuf_.read(&Error, 1);
					inBuf_.read(&sid_, 1);
					doc["type"] = "login";
					doc["err"] = Error;
					const auto login_msg = to_string(doc);
					SendToClient(login_msg);
				} break;

				case msgTypeLogout: {
					// std::cout << "msgTypeLogout" << std::endl;
				} break;

				case msgTypeTermData: {
					if(waiting_for_bytes_) {
						inBuf_.read(&scratch_[0], inBuf_.used());
						SendToClient((u_char *)&scratch_[0], (int) inBuf_.used());
						waiting_for_bytes_ -= inBuf_.used();
						done=true;
					} else {
						inBuf_.read(&scratch_[0], MsgLen);
						SendToClient((u_char *)&scratch_[0], (int) MsgLen);
					}
				} break;

				case msgTypeWinsize: {
					// std::cout << "msgTypeWinsize" << std::endl;
				} break;

				case msgTypeCmd: {
					// std::cout << "msgTypeCmd" << std::endl;
				} break;

				case msgTypeHeartbeat: {
					// std::cout << "msgTypeHeartbeat: " << MsgLen << " bytes" << std::endl;
					// PrintBuf(&inBuf[0], len);
					u_char MsgBuf[32]{0};
					MsgBuf[0] = msgTypeHeartbeat;
					socket_.sendBytes(MsgBuf, 3);
				} break;

				case msgTypeFile: {
					// std::cout << "msgTypeFile" << std::endl;
				} break;

				case msgTypeHttp: {
					// std::cout << "msgTypeHttp" << std::endl;
				} break;

				case msgTypeAck: {
					// std::cout << "msgTypeAck" << std::endl;
				} break;

				case msgTypeMax: {
					// std::cout << "msgTypeMax" << std::endl;
				} break;
				}
			}
/*
			} else {
				Logger().debug(fmt::format("DeRegistration: {} shutting down session {}.", serial_, id_));
				return delete this;
			}
*/
		}
		catch (const Poco::Exception & E)
		{
			Logger().debug(fmt::format("DeRegistration: {} exception, session {}.", serial_, id_));
			Logger().log(E);
			return delete this;
		}
	}

	void RTTY_Device_ConnectionHandler::onSocketShutdown([[maybe_unused]] const Poco::AutoPtr<Poco::Net::ShutdownNotification>& pNf)
	{
		std::cout << "Device " << id_ << " closing socket." << std::endl;
		delete this;
	}
}