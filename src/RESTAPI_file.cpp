//
// Created by stephane bourque on 2021-03-29.
//

#include "RESTAPI_file.h"

#include "uAuthService.h"
#include "uFileUploader.h"
#include "Poco/File.h"

#include <fstream>

void RESTAPI_file::handleRequest(HTTPServerRequest& Request, HTTPServerResponse& Response)
{
//    if(!ContinueProcessing(Request,Response))
//        return;

//    if(!IsAuthorized(Request,Response))
//        return;

    try {
        ParseParameters(Request);

        std::cout << __LINE__ << std::endl;

        if (Request.getMethod() == Poco::Net::HTTPRequest::HTTP_GET) {
            auto UUID = GetBinding("uuid", "");

            std::cout << __LINE__ << std::endl;


            //does the file exist
            Poco::File  DownloadFile(uCentral::uFileUploader::Path() + "/" + UUID);
            std::cout << __LINE__ << std::endl;

            if(!DownloadFile.isFile())
            {
                NotFound(Response);
                return;
            }

            Response.set("Content-Type","application/octet-stream");
            Response.set("Content-Disposition", "attachment; filename=" + UUID);
            Response.set("Content-Transfer-Encoding","binary");
            Response.set("Accept-Ranges", "bytes");
            Response.set("Cache-Control", "private");
            Response.set("Pragma", "private");
            Response.set("Expires", "Mon, 26 Jul 2027 05:00:00 GMT");
            Response.set("Content-Length", std::to_string(DownloadFile.getSize()));
            Response.sendFile(DownloadFile.path(),"application/octet-stream");

            return;

        } else if (Request.getMethod() == Poco::Net::HTTPRequest::HTTP_DELETE) {
            auto UUID = GetBinding("uuid", "");

            //does the file exist
            Poco::File  DownloadFile(uCentral::uFileUploader::Path() + "/" + UUID);

            if(DownloadFile.isFile())
            {
                DownloadFile.remove();
                OK(Response);
            }
            else
            {
                NotFound(Response);
            }
        }
        return;
    }
    catch(const Poco::Exception &E)
    {
        Logger_.error(Poco::format("%s: failed with %s",std::string(__func__), E.displayText()));
    }
    BadRequest(Response);
}