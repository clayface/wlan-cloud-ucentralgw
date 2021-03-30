//
// Created by stephane bourque on 2021-03-29.
//

#ifndef UCENTRAL_UFILEUPLOADER_H
#define UCENTRAL_UFILEUPLOADER_H

#include "SubSystemServer.h"

#include "Poco/Net/HTTPServer.h"
#include "Poco/Net/HTTPRequestHandler.h"
#include "Poco/Net/HTTPRequestHandlerFactory.h"
#include "Poco/Net/HTTPServerParams.h"
#include "Poco/Net/HTTPServerRequest.h"
#include "Poco/Net/HTTPServerResponse.h"
#include "Poco/Net/HTTPServerParams.h"
#include "Poco/Net/ServerSocket.h"
#include "Poco/Net/SecureServerSocket.h"
#include "Poco/Net/WebSocket.h"
#include "Poco/Net/NetException.h"
#include "Poco/Net/Context.h"
#include "Poco/JSON/Parser.h"
#include "Poco/DynamicAny.h"
#include "Poco/Net/HTMLForm.h"
#include "Poco/Net/PartHandler.h"
#include "Poco/Net/MessageHeader.h"
#include "Poco/CountingStream.h"
#include "Poco/StreamCopier.h"
#include "Poco/Exception.h"

namespace uCentral::uFileUploader {
    int Start();
    void Stop();
    const std::string & FullName();
    bool AddUUID( const std::string & UUID);
    bool ValidRequest(const std::string & UUID);
    void RemoveRequest(const std::string &UUID);
    const std::string & Path();

    class Service : public SubSystemServer {
    public:
        Service() noexcept;

        friend int Start();
        friend void Stop();
        friend const std::string & FullName();
        friend bool AddUUID( const std::string & UUID);
        friend bool ValidRequest(const std::string & UUID);
        friend void RemoveRequest(const std::string &UUID);
        friend const std::string & Path();

        static Service *instance() {
            if (instance_ == nullptr) {
                instance_ = new Service;
            }
            return instance_;
        }

    private:
        int Start() override;
        void Stop() override;
        const std::string & FullName();
        bool AddUUID( const std::string & UUID);
        bool ValidRequest(const std::string & UUID);
        void RemoveRequest(const std::string &UUID);
        const std::string & Path() { return Path_; };

        static Service *instance_;
        std::mutex                              Mutex_;
        std::vector<std::unique_ptr<Poco::Net::HTTPServer>>   Servers_;
        std::string                     FullName_;
        std::map<std::string,uint64_t>  OutStandingUploads_;
        std::string                     Path_;
    };

    class RequestHandlerFactory : public Poco::Net::HTTPRequestHandlerFactory {
    public:
        RequestHandlerFactory() :
                Logger_(Service::instance()->Logger()){}

        Poco::Net::HTTPRequestHandler *createRequestHandler(const Poco::Net::HTTPServerRequest &request) override;
    private:
        Poco::Logger    & Logger_;
    };

}; //   namespace

#endif //UCENTRAL_UFILEUPLOADER_H